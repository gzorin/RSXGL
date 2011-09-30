// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// clear.c - GL functions pertaining to clearing of the framebuffer.

#include <GL3/gl3.h>

#include "gcm.h"
#include "rsxgl_context.h"
#include "gl_fifo.h"
#include "nv40.h"
#include "error.h"
#include "framebuffer.h"
#include "state.h"

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

//
static inline float
clampf(float x)
{
  if(x < 0) {
    return 0.0f;
  }
  else if(x > 1.0) {
    return 1.0f;
  }
  else {
    return x;
  }
}

GLAPI void APIENTRY
glClearColor(GLclampf red,GLclampf green,GLclampf blue,GLclampf alpha)
{
  struct rsxgl_context_t * ctx = current_ctx();

  ctx -> state.color.clear =
    (uint32_t)(red * 255.0f) << 16 |
    (uint32_t)(green * 255.0f) << 8 |
    (uint32_t)(blue * 255.0f) << 0 |
    (uint32_t)(alpha * 255.0f) << 24;

  ctx -> state.invalid.parts.the_rest = 1;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glClearDepthf(GLclampf d)
{
  struct rsxgl_context_t * ctx = current_ctx();

  switch(ctx -> base.config -> egl_depth_size) {
  case 16:
    ctx -> state.depth.clear = clampf(d) * 65535.0f;
    break;
  case 24:
    ctx -> state.depth.clear = clampf(d) * 16777215.0f;
    break;
  default:
    break;
  }

  ctx -> state.invalid.parts.the_rest = 1;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glClearStencil (GLint s)
{
  struct rsxgl_context_t * ctx = current_ctx();

  switch(ctx -> base.config -> egl_depth_size) {
  case 16:
    ctx -> state.stencil.clear = s;
    break;
  default:
    break;
  }

  ctx -> state.invalid.parts.the_rest = 1;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glClear(GLbitfield mask)
{
  if(mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  struct rsxgl_context_t * ctx = current_ctx();
  
  const uint32_t timestamp = rsxgl_timestamp_create(ctx);
  rsxgl_draw_framebuffer_validate(ctx,timestamp);
  rsxgl_state_validate(ctx);

  gcmContextData * context = ctx -> base.gcm_context;

  uint32_t * buffer = gcm_reserve(context,2);
  gcm_emit_method(&buffer,NV30_3D_CLEAR_BUFFERS,1);
  
  gcm_emit(&buffer,
	   (mask & GL_COLOR_BUFFER_BIT ? ((ctx -> state.write_mask.r ? NV30_3D_CLEAR_BUFFERS_COLOR_R : 0) |
					  (ctx -> state.write_mask.g ? NV30_3D_CLEAR_BUFFERS_COLOR_G : 0) |
					  (ctx -> state.write_mask.b ? NV30_3D_CLEAR_BUFFERS_COLOR_B : 0) |
					  (ctx -> state.write_mask.a ? NV30_3D_CLEAR_BUFFERS_COLOR_A : 0)) : 0) |
	   (mask & GL_DEPTH_BUFFER_BIT ? (ctx -> state.write_mask.depth ? NV30_3D_CLEAR_BUFFERS_DEPTH : 0) : 0) |
	   (mask & GL_STENCIL_BUFFER_BIT ? NV30_3D_CLEAR_BUFFERS_STENCIL : 0));
  
  gcm_finish_commands(context,&buffer);

  rsxgl_timestamp_post(ctx,timestamp);
  
  RSXGL_NOERROR_();
}
