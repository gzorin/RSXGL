#include "debug.h"
#include "rsxgl_context.h"
#include "framebuffer.h"
#include "migrate.h"
#include "nv40.h"
#include "timestamp.h"
#include "rsxgl_limits.h"
#include "cxxutil.h"
#include "GL3/rsxgl.h"

#include <malloc.h>
#include <stdint.h>
#include <string.h>

rsxgl_context_t * rsxgl_ctx = 0;

extern "C"
void *
rsxgl_context_create(const struct rsxegl_config_t * config,gcmContextData * gcm_context)
{
  return new rsxgl_context_t(config,gcm_context);
}

rsxgl_context_t::rsxgl_context_t(const struct rsxegl_config_t * config,gcmContextData * gcm_context)
  : active_texture(0), ref(0), timestamp_sync(0), next_timestamp(1), last_timestamp(0), cached_timestamp(0)
{
  base.api = EGL_OPENGL_API;
  base.config = config;
  base.gcm_context = gcm_context;
  base.draw = 0;
  base.read = 0;
  base.valid = 1;
  base.callback = rsxgl_context_t::egl_callback;

  timestamp_sync = rsxgl_sync_object_allocate();
  rsxgl_assert(timestamp_sync != 0);
  rsxgl_sync_cpu_signal(timestamp_sync,0);
}

static inline void
rsxgl_make_context_current(rsxgl_context_t * ctx)
{
  ctx -> state.invalid.all = ~0;
  
  ctx -> invalid_attribs.set();
  ctx -> invalid_textures.set();
  ctx -> invalid_samplers.set();

  rsxgl_ctx = ctx;
}

void
rsxgl_context_t::egl_callback(struct rsxegl_context_t * egl_ctx,const uint8_t op)
{
  rsxgl_context_t * ctx = (rsxgl_context_t *)egl_ctx;

  if(op == RSXEGL_MAKE_CONTEXT_CURRENT) {
    if(ctx -> state.viewport.width == 0 && ctx -> state.viewport.height == 0) {
      ctx -> state.viewport.x = 0;
      ctx -> state.viewport.y = 0;
      ctx -> state.viewport.width = ctx -> base.draw -> width;
      ctx -> state.viewport.height = ctx -> base.draw -> height;
      ctx -> state.viewport.depthRange[0] = 0.0f;
      ctx -> state.viewport.depthRange[1] = 1.0f;
    }
    rsxgl_make_context_current(ctx);
  }
  else if(op == RSXEGL_POST_GPU_SWAP) {
    rsxgl_migrate_reset(ctx -> base.gcm_context);
#if 0
    //
    ctx -> state.invalid.all = ~0;
    
    ctx -> invalid_attribs.set();
    ctx -> invalid_textures.set();
    ctx -> invalid_samplers.set();
#endif
  }
  else if(op == RSXEGL_DESTROY_CONTEXT) {
    ctx -> base.valid = 0;
  }
}

uint32_t
rsxgl_timestamp_create(rsxgl_context_t * ctx)
{
  const uint32_t max_timestamp = RSXGL_MAX_TIMESTAMP;
  rsxgl_assert(is_pot(max_timestamp + 1));

  const uint32_t result = ctx -> next_timestamp;
  rsxgl_assert(result > 0);

  const uint32_t next_timestamp = result + 1;
  
  if(next_timestamp & ~max_timestamp || next_timestamp == 0) {
    // Block until the last timestamp has been reached:
    rsxgl_timestamp_wait(ctx -> cached_timestamp,ctx -> timestamp_sync,ctx -> last_timestamp,RSXGL_SYNC_SLEEP_INTERVAL);
    
    // Reset the timestamps of all timestamp-able objects:
    //
    // Buffers:
    {
      const buffer_t::name_type n = buffer_t::storage().contents().size;
      for(buffer_t::name_type i = 0;i < n;++i) {
	if(!buffer_t::storage().is_object(i)) continue;
	buffer_t::storage().at(i).timestamp = 0;
      }
    }
    
    // Textures:
    {
      const texture_t::name_type n = texture_t::storage().contents().size;
      for(texture_t::name_type i = 0;i < n;++i) {
	if(!texture_t::storage().is_object(i)) continue;
	texture_t::storage().at(i).timestamp = 0;
      }
    }

    //
    ctx -> cached_timestamp = 0;

    ctx -> next_timestamp = 1;
  }
  else {
    ctx -> next_timestamp = next_timestamp;
  }

  return result;
}

void
rsxgl_timestamp_wait(rsxgl_context_t * ctx,const uint32_t timestamp)
{
  rsxgl_assert(ctx -> timestamp_sync != 0);

  rsxgl_gcm_flush(ctx -> gcm_context());
  rsxgl_timestamp_wait(ctx -> cached_timestamp,ctx -> timestamp_sync,timestamp,RSXGL_SYNC_SLEEP_INTERVAL);
}

void
rsxgl_timestamp_post(rsxgl_context_t * ctx,const uint32_t timestamp)
{
  rsxgl_assert(ctx -> timestamp_sync != 0);

  rsxgl_emit_sync_gpu_signal_write(ctx -> base.gcm_context,ctx -> timestamp_sync,timestamp);
  ctx -> last_timestamp = timestamp;
}

#if 0
// librsx compatibility functions:
extern "C" void *
rsxglCreateContext(void * gcm_context)
{
  if(gcm_context != 0) {
    return rsxgl_context_create(0,(gcmContextData *)gcm_context);
  }
  else {
    return 0;
  }
}

extern "C" void
rsxglSetSurface(void * context,void * surface,uint8_t buffer)
{
}

extern "C" void
rsxglMakeCurrent(void * context)
{
  rsxgl_make_context_current((rsxgl_context_t *)context);
}

extern "C" void
rsxglPreSwap()
{
  if(rsxgl_ctx != 0) {
    rsxgl_context_t::egl_callback((rsxegl_context_t *)rsxgl_ctx,(uint8_t)RSXEGL_POST_CPU_SWAP);
  }
}

extern "C" void
rsxglPostSwap()
{
  if(rsxgl_ctx != 0) {
    rsxgl_context_t::egl_callback((rsxegl_context_t *)rsxgl_ctx,(uint8_t)RSXEGL_POST_GPU_SWAP);
  }
}

extern "C" void
rsxglDestroyContext(void * gcm_context)
{
  if(gcm_context != 0) {
    rsxgl_context_t::egl_callback((rsxegl_context_t *)rsxgl_ctx,(uint8_t)RSXEGL_DESTROY_CONTEXT);
    delete (rsxgl_context_t *)gcm_context;
  }
}
#endif
