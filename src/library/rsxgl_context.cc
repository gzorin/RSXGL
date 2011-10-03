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
  : draw_buffer(0), active_texture(0), draw_status(0), read_status(0), ref(0), timestamp_sync(0), next_timestamp(1), last_timestamp(0), cached_timestamp(0)
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
    ctx -> surfaces_format = ctx -> base.draw -> format;

    // formats are unused when validating framebuffer 0 (the EGL-supplied framebuffer):
    ctx -> color_surfaces[0].format = ~0;
    ctx -> color_surfaces[0].size[0] = ctx -> base.draw -> width;
    ctx -> color_surfaces[0].size[1] = ctx -> base.draw -> height;
    ctx -> color_surfaces[0].pitch = ctx -> base.draw -> color_pitch;
    ctx -> color_surfaces[0].memory.location = ctx -> base.draw -> color_buffer[0].location;
    ctx -> color_surfaces[0].memory.offset = ctx -> base.draw -> color_buffer[0].offset;
    ctx -> color_surfaces[0].memory.owner = 0;

    ctx -> color_surfaces[1].format = ~0;
    ctx -> color_surfaces[1].size[0] = ctx -> base.draw -> width;
    ctx -> color_surfaces[1].size[1] = ctx -> base.draw -> height;
    ctx -> color_surfaces[1].pitch = ctx -> base.draw -> color_pitch;
    ctx -> color_surfaces[1].memory.location = ctx -> base.draw -> color_buffer[1].location;
    ctx -> color_surfaces[1].memory.offset = ctx -> base.draw -> color_buffer[1].offset;
    ctx -> color_surfaces[1].memory.owner = 0;

    ctx -> depth_surface.format = ~0;
    ctx -> depth_surface.size[0] = ctx -> base.draw -> width;
    ctx -> depth_surface.size[1] = ctx -> base.draw -> height;
    ctx -> depth_surface.pitch = ctx -> base.draw -> depth_pitch;
    ctx -> depth_surface.memory.location = ctx -> base.draw -> depth_buffer.location;
    ctx -> depth_surface.memory.offset = ctx -> base.draw -> depth_buffer.offset;
    ctx -> depth_surface.memory.owner = 0;

    ctx -> draw_buffer = ctx -> base.draw -> buffer;

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
    ctx -> draw_buffer = ctx -> base.draw -> buffer;
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

uint32_t
rsxgl_draw_status_validate(rsxgl_context_t * ctx)
{
  if(ctx -> state.invalid.draw_status) {
    uint8_t draw_status = 1;

    // Is there a valid program bound?
    draw_status &= ctx -> program_binding.is_anything_bound(RSXGL_ACTIVE_PROGRAM) && program_binding[RSXGL_ACTIVE_PROGRAM].validated;

    // Writing to 

    ctx -> state.invalid.draw_status = 0;
  }
  return ctx -> draw_status;
}

uint32_t
rsxgl_read_status_validate(rsxgl_context_t * ctx)
{
  if(ctx -> state.invalid.read_status) {
    ctx -> state.invalid.read_status = 0;
  }
  return ctx -> read_status;
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
