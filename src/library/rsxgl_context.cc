#include "debug.h"
#include "rsxgl_context.h"
#include "egl_types.h"
#include "framebuffer.h"
#include "migrate.h"
#include "nv40.h"
#include "timestamp.h"
#include "rsxgl_limits.h"
#include "cxxutil.h"

#include <malloc.h>
#include <stdint.h>
#include <string.h>

rsxgl_context_t * rsxgl_ctx = 0;

extern "C"
void *
rsxgl_context_create(const struct rsxegl_config_t * config)
{
  rsxgl_context_t * ctx = new rsxgl_context_t();

  ctx -> base.api = EGL_OPENGL_API;
  ctx -> base.config = config;
  ctx -> base.gcm_context = 0;
  ctx -> base.draw = 0;
  ctx -> base.read = 0;
  ctx -> base.valid = 1;
  ctx -> base.callback = rsxgl_context_t::egl_callback;
  
  ctx -> timestamp_sync = rsxgl_sync_object_allocate();
  rsxgl_assert(ctx -> timestamp_sync != 0);
  rsxgl_sync_cpu_signal(ctx -> timestamp_sync,0);
  ctx -> cached_timestamp = 0;

  return ctx;
}

void
rsxgl_context_t::egl_callback(struct rsxegl_context_t * egl_ctx,const uint8_t op)
{
  rsxgl_context_t * ctx = (rsxgl_context_t *)egl_ctx;

  if(op == RSXEGL_MAKE_CONTEXT_CURRENT) {
    ctx -> state.colorSurface.surface = 0;
    ctx -> state.colorSurface.location = 0;
    ctx -> state.colorSurface.offset = ctx -> base.draw -> color_addr[ctx -> base.draw -> buffer];
    ctx -> state.colorSurface.pitch = ctx -> base.draw -> color_pitch;
    
    ctx -> state.depthSurface.surface = 4;
    ctx -> state.depthSurface.location = 0;
    ctx -> state.depthSurface.offset = ctx -> base.draw -> depth_addr;
    ctx -> state.depthSurface.pitch = ctx -> base.draw -> depth_pitch;
    
    ctx -> state.format.enabled = NV30_3D_RT_ENABLE_COLOR0;
    ctx -> state.format.format = ctx -> base.draw -> format;
    ctx -> state.format.width = ctx -> base.draw -> width;
    ctx -> state.format.height = ctx -> base.draw -> height;
    
    if(ctx -> state.viewport.width == 0 && ctx -> state.viewport.height == 0) {
      ctx -> state.viewport.x = 0;
      ctx -> state.viewport.y = 0;
      ctx -> state.viewport.width = ctx -> base.draw -> width;
      ctx -> state.viewport.height = ctx -> base.draw -> height;
      ctx -> state.viewport.depthRange[0] = 0.0f;
      ctx -> state.viewport.depthRange[1] = 1.0f;
    }
    
    // ctx -> next_timestamp = 1;
    // ctx -> cached_timestamp = 0;
    
    //
    rsxgl_surface_emit(ctx -> base.gcm_context,&ctx -> state.colorSurface);
    rsxgl_surface_emit(ctx -> base.gcm_context,&ctx -> state.depthSurface);
    rsxgl_format_emit(ctx -> base.gcm_context,&ctx -> state.format);
    
    ctx -> state.invalid.all = ~0;
    
    ctx -> invalid_attribs.set();
    ctx -> invalid_textures.set();
    ctx -> invalid_samplers.set();
    
    rsxgl_state_validate(ctx);
    
    rsxgl_ctx = ctx;
  }
  else if(op == RSXEGL_POST_GPU_SWAP) {
    rsxgl_migrate_reset(ctx -> base.gcm_context);
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
  //rsxgl_debug_printf("waiting on timestamp: %u\n",timestamp);

  rsxgl_assert(ctx -> timestamp_sync != 0);

  rsxgl_gcm_flush(ctx -> gcm_context());
  rsxgl_timestamp_wait(ctx -> cached_timestamp,ctx -> timestamp_sync,timestamp,RSXGL_SYNC_SLEEP_INTERVAL);
}

void
rsxgl_timestamp_post(rsxgl_context_t * ctx,const uint32_t timestamp)
{
  rsxgl_assert(ctx -> timestamp_sync != 0);

  rsxgl_emit_sync_gpu_signal_read(ctx -> base.gcm_context,ctx -> timestamp_sync,timestamp);
  ctx -> last_timestamp = timestamp;

  //rsxgl_debug_printf("posted timestamp: %u\n",timestamp);
}
