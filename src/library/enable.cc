// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// enable.c - glEnable and glDisable functions.

#include <GL3/gl3.h>

#include "rsxgl_context.h"
#include "gl_constants.h"
#include "error.h"

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

GLAPI void APIENTRY
glEnable (GLenum cap)
{
  struct rsxgl_context_t * ctx = current_ctx();
  switch(cap) {
  case GL_SCISSOR_TEST:
    ctx -> state.enable.scissor = 1;
    ctx -> state.invalid.parts.scissor = 1;
    break;
  case GL_DEPTH_TEST:
    ctx -> state.enable.depth_test = 1;
    break;
  case GL_BLEND:
    ctx -> state.enable.blend = 1;
    break;
  case GL_CULL_FACE:
    ctx -> state.polygon.cullEnable = 1;
    break;
  case GL_STENCIL_TEST:
    ctx -> state.stencil.face[0].enable = 1;
    ctx -> state.stencil.face[1].enable = 1;
    break;
  case GL_PRIMITIVE_RESTART:
    ctx -> state.enable.primitive_restart = 1;
    ctx -> state.invalid.parts.primitive_restart = 1;
    RSXGL_NOERROR_();

  default:
    RSXGL_ERROR_(GL_INVALID_ENUM);
  };

  ctx -> state.invalid.parts.the_rest = 1;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDisable (GLenum cap)
{
  struct rsxgl_context_t * ctx = current_ctx();
  switch(cap) {
  case GL_SCISSOR_TEST:
    ctx -> state.enable.scissor = 0;
    ctx -> state.invalid.parts.scissor = 1;
    break;
  case GL_DEPTH_TEST:
    ctx -> state.enable.depth_test = 0;
    break;
  case GL_BLEND:
    ctx -> state.enable.blend = 0;
    break;
  case GL_CULL_FACE:
    ctx -> state.polygon.cullEnable = 0;
    break;
  case GL_STENCIL_TEST:
    ctx -> state.stencil.face[0].enable = 0;
    ctx -> state.stencil.face[1].enable = 0;
    break;
  case GL_PRIMITIVE_RESTART:
    ctx -> state.enable.primitive_restart = 0;
    ctx -> state.invalid.parts.primitive_restart = 1;
    RSXGL_NOERROR_();

  default:
    RSXGL_ERROR_(GL_INVALID_ENUM);
  };

  ctx -> state.invalid.parts.the_rest = 1;

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsEnabled (GLenum cap)
{
  struct rsxgl_context_t * ctx = current_ctx();
  switch(cap) {
  case GL_SCISSOR_TEST:
    RSXGL_NOERROR(ctx -> state.enable.scissor);
    break;
  case GL_DEPTH_TEST:
    RSXGL_NOERROR(ctx -> state.enable.depth_test);
    break;
  case GL_BLEND:
    RSXGL_NOERROR(ctx -> state.enable.blend);
    break;
  case GL_CULL_FACE:
    RSXGL_NOERROR(ctx -> state.polygon.cullEnable);
    break;
  case GL_STENCIL_TEST:
    RSXGL_NOERROR(ctx -> state.stencil.face[0].enable && ctx -> state.stencil.face[1].enable);
    break;
  case GL_PRIMITIVE_RESTART:
    RSXGL_NOERROR(ctx -> state.enable.primitive_restart);
    break;
  default:
    RSXGL_ERROR(GL_INVALID_ENUM,GL_FALSE);
  };

  RSXGL_NOERROR(GL_FALSE);
}
