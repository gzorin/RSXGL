// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// get.cc - Implement glGet*() functions.

#include <GL3/gl3.h>

#include "rsxgl_context.h"
#include "error.h"
#include "gl_constants.h"

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

template< typename T >
static inline void
rsxgl_get(rsxgl_context_t * ctx,const GLenum pname, T * params)
{
  if(pname == GL_MAX_TEXTURE_IMAGE_UNITS) {
    *params = RSXGL_MAX_TEXTURE_IMAGE_UNITS;
  }
  else if(pname == GL_MAX_TEXTURE_SIZE) {
    *params = RSXGL_MAX_TEXTURE_SIZE;
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetBooleanv (GLenum pname, GLboolean *params)
{
  rsxgl_get(current_ctx(),pname,params);
}

GLAPI void APIENTRY
glGetDoublev (GLenum pname, GLdouble *params)
{
  rsxgl_get(current_ctx(),pname,params);
}

GLAPI void APIENTRY
glGetFloatv (GLenum pname, GLfloat *params)
{
  rsxgl_get(current_ctx(),pname,params);
}

GLAPI void APIENTRY
glGetIntegerv (GLenum pname, GLint *params)
{
  rsxgl_get(current_ctx(),pname,params);
}

GLAPI const GLubyte * APIENTRY
glGetString (GLenum name)
{
  if(name == GL_VENDOR) {
    RSXGL_NOERROR((const GLubyte *)"RSXGL");
  }
  else if(name == GL_RENDERER) {
    RSXGL_NOERROR((const GLubyte *)"NV40");
  }
  else if(name == GL_VERSION) {
    RSXGL_NOERROR((const GLubyte *)"3.1.0 RSXGL");
  }
  else if(name == GL_SHADING_LANGUAGE_VERSION) {
    RSXGL_NOERROR((const GLubyte *)"1.30");
  }
  else if(name == GL_EXTENSIONS) {
    RSXGL_NOERROR((const GLubyte *)"");
  }
  else {
    RSXGL_ERROR(GL_INVALID_ENUM,0);
  }
}

GLAPI const GLubyte * APIENTRY
glGetStringi (GLenum name, GLuint index)
{
  if(name == GL_EXTENSIONS) {
    RSXGL_ERROR(GL_INVALID_VALUE,0);
  }
  else {
    RSXGL_ERROR(GL_INVALID_ENUM,0);
  }
}
