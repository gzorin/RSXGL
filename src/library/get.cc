// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// get.cc - Implement glGet*() functions.

#include <GL3/gl3.h>

#include "rsxgl_context.h"
#include "error.h"

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

GLAPI void APIENTRY
glGetBooleanv (GLenum pname, GLboolean *params)
{
}

GLAPI void APIENTRY
glGetDoublev (GLenum pname, GLdouble *params)
{
}

GLAPI void APIENTRY
glGetFloatv (GLenum pname, GLfloat *params)
{
}

GLAPI void APIENTRY
glGetIntegerv (GLenum pname, GLint *params)
{
}

GLAPI void APIENTRY
glGetInteger64v (GLenum pname, GLint64 *params)
{
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
    RSXGL_NOERROR((const GLubyte *)"3.1");
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
