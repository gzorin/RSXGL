#include "rsxgl_context.h"

#include "debug.h"
#include "framebuffer.h"
#include "migrate.h"
#include "nv40.h"
#include "timestamp.h"
#include "rsxgl_limits.h"
#include "cxxutil.h"

#include <GL3/gl3.h>
#include "GL3/rsxgl.h"

#include <rsx/gcm_sys.h>

extern "C" {

  struct pipe_context *
  nvfx_create(struct pipe_screen *pscreen, void *priv);

}

#include <malloc.h>
#include <stdint.h>
#include <string.h>

rsxgl_context_t * rsxgl_ctx = 0;

extern "C"
void *
rsxgl_context_create(const struct rsxegl_config_t * config,gcmContextData * gcm_context,struct pipe_screen * screen,rsxgl_object_context_t * object_context)
{
  rsxgl_debug_printf("%s\n",__PRETTY_FUNCTION__);
  return new rsxgl_context_t(config,gcm_context,screen,object_context);
}

extern "C"
void *
rsxgl_object_context_create()
{
  return new rsxgl_object_context_t();
}

rsxgl_context_t::rsxgl_context_t(const struct rsxegl_config_t * config,gcmContextData * gcm_context,struct pipe_screen * screen,struct rsxgl_object_context_t * _object_context)
  : m_object_context(_object_context), active_texture(0), any_samples_passed_query(RSXGL_MAX_QUERY_OBJECTS), ref(0), timestamp_sync(0), next_timestamp(1), last_timestamp(0), cached_timestamp(0), m_compiler_context(0)
{
  base.api = EGL_OPENGL_API;
  base.config = config;
  base.gcm_context = gcm_context;
  base.draw = 0;
  base.read = 0;
  base.valid = 1;
  base.callback = rsxgl_context_t::egl_callback;
  base.screen = screen;
  base.sync_sleep_interval = RSXGL_SYNC_SLEEP_INTERVAL;

  m_pctx = nvfx_create(screen,0);
  rsxgl_debug_printf("m_pctx: %lx\n",(unsigned long)m_pctx);

  ++m_object_context -> m_refCount;

  timestamp_sync = rsxgl_sync_object_allocate();
  rsxgl_assert(timestamp_sync != 0);
  rsxgl_sync_cpu_signal(timestamp_sync,0);
}

rsxgl_context_t::~rsxgl_context_t()
{
  --m_object_context -> m_refCount;
  if(m_object_context -> m_refCount == 0) {
    delete m_object_context;
  }

  if(m_compiler_context != 0) {
    delete m_compiler_context;
  }
}

void
rsxgl_context_t::egl_callback(struct rsxegl_context_t * egl_ctx,const uint8_t op)
{
  rsxgl_context_t * ctx = (rsxgl_context_t *)egl_ctx;

  if(op == RSXEGL_MAKE_CONTEXT_CURRENT || op == RSXEGL_POST_GPU_SWAP) {
    framebuffer_t & framebuffer = ctx -> object_context() -> framebuffer_storage().at(0);

    if(op == RSXEGL_MAKE_CONTEXT_CURRENT) {
      framebuffer.attachment_types.set(RSXGL_COLOR_ATTACHMENT0,ctx -> base.draw -> color_pformat != PIPE_FORMAT_NONE ? RSXGL_ATTACHMENT_TYPE_RENDERBUFFER : RSXGL_ATTACHMENT_TYPE_NONE);
      framebuffer.attachment_types.set(RSXGL_DEPTH_STENCIL_ATTACHMENT,ctx -> base.draw -> depth_pformat != PIPE_FORMAT_NONE ? RSXGL_ATTACHMENT_TYPE_RENDERBUFFER : RSXGL_ATTACHMENT_TYPE_NONE);

      if(ctx -> state.viewport.width == 0 && ctx -> state.viewport.height == 0) {
	ctx -> state.viewport.x = 0;
	ctx -> state.viewport.y = 0;
	ctx -> state.viewport.width = ctx -> base.draw -> width;
	ctx -> state.viewport.height = ctx -> base.draw -> height;
	ctx -> state.viewport.depthRange[0] = 0.0f;
	ctx -> state.viewport.depthRange[1] = 1.0f;
      }

      rsxgl_ctx = ctx;
    }

    //
    framebuffer.invalid = 1;
    framebuffer.invalid_complete = 1;

    ctx -> state.invalid.all = ~0;
    ctx -> invalid.all = ~0;
    
    ctx -> invalid_attribs.set();
    ctx -> invalid_textures.set();
    ctx -> invalid_samplers.set();
  }
  else if(op == RSXEGL_DESTROY_CONTEXT) {
    ctx -> base.valid = 0;
  }
}

uint32_t
rsxgl_timestamp_create(rsxgl_context_t * ctx,const uint32_t count)
{
  const uint32_t max_timestamp = RSXGL_MAX_TIMESTAMP;

  const uint32_t current_timestamp = ctx -> next_timestamp;
  rsxgl_assert(current_timestamp == (ctx -> last_timestamp + 1));

  const uint32_t next_timestamp = current_timestamp + count;

  // check for overflow:
  if(next_timestamp > max_timestamp || next_timestamp < current_timestamp) {
    // block until last_timestamp is reached:
    rsxgl_timestamp_wait(ctx -> cached_timestamp,ctx -> timestamp_sync,ctx -> last_timestamp,ctx -> base.sync_sleep_interval);

    // Buffers:
    {
      const buffer_t::name_type n = ctx -> object_context() -> buffer_storage().contents().size;
      for(buffer_t::name_type i = 0;i < n;++i) {
	if(!ctx -> object_context() -> buffer_storage().is_object(i)) continue;
	ctx -> object_context() -> buffer_storage().at(i).timestamp = 0;
      }
    }
    
    // Textures:
    {
      const texture_t::name_type n = ctx -> object_context() -> texture_storage().contents().size;
      for(texture_t::name_type i = 0;i < n;++i) {
	if(!ctx -> object_context() -> texture_storage().is_object(i)) continue;
	ctx -> object_context() -> texture_storage().at(i).timestamp = 0;
      }
    }

    //
    ctx -> cached_timestamp = 0;
    ctx -> next_timestamp = 1 + count;
    return 1;
  }
  //
  else {
    ctx -> next_timestamp = next_timestamp;
    return current_timestamp;
  }
}

void
rsxgl_timestamp_post(rsxgl_context_t * ctx,const uint32_t timestamp)
{
  rsxgl_assert(ctx -> timestamp_sync != 0);

  rsxgl_emit_sync_gpu_signal_write(ctx -> base.gcm_context,ctx -> timestamp_sync,timestamp);
  ctx -> last_timestamp = timestamp;
}

void
rsxgl_timestamp_wait(rsxgl_context_t * ctx,const uint32_t timestamp)
{
  rsxgl_assert(ctx -> timestamp_sync != 0);

  rsxgl_gcm_flush(ctx -> gcm_context());
  rsxgl_timestamp_wait(ctx -> cached_timestamp,ctx -> timestamp_sync,timestamp,ctx -> base.sync_sleep_interval);
}

bool
rsxgl_timestamp_passed(rsxgl_context_t * ctx,const uint32_t timestamp)
{
  rsxgl_assert(ctx -> timestamp_sync != 0);

  rsxgl_gcm_flush(ctx -> gcm_context());
  return rsxgl_timestamp_passed(ctx -> cached_timestamp,ctx -> timestamp_sync,timestamp);
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
  ctx -> state.invalid.all = ~0;
  ctx -> invalid.all = ~0;
  
  ctx -> invalid_attribs.set();
  ctx -> invalid_textures.set();
  ctx -> invalid_samplers.set();

  rsxgl_ctx = ctx;  
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
