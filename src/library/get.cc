// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// get.cc - Implement glGet*() functions.

#include "rsxgl_context.h"

#include <GL3/gl3.h>

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
