// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// error.c - Retrieve the current OpenGL error code.

#include <rsx/gcm_sys.h>

#include <GL3/gl3.h>

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

// Error stuff:
GLenum rsxgl_error = GL_NO_ERROR;

GLAPI GLenum APIENTRY
glGetError (void)
{
  GLenum e = rsxgl_error;
  rsxgl_error = GL_NO_ERROR;
  return e;
}
