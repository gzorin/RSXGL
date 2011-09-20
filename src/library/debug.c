// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// debug.cc - Facility for the library to report debugging information.

#include "debug.h"

#include <GL3/gl3.h>
#include "GL3/rsxgl3ext.h"

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdlib.h>

static char * debug_buffer = 0;
static size_t ndebug_buffer = 0;

static void (*debug_printf_callback)(GLsizei,const GLchar *) = 0;

void
rsxgl_debug_printf(const char * fmt,...)
{
  if(debug_buffer != 0 && debug_printf_callback != 0) {
    va_list ap;
    va_start(ap,fmt);
    size_t n = vsnprintf(debug_buffer,ndebug_buffer,fmt,ap);
    va_end(ap);

    (*debug_printf_callback)((n < ndebug_buffer) ? n : ndebug_buffer,debug_buffer);
  }
}

// Based upon newlib's __assert_func()
void
__rsxgl_assert_func(const char * file,int line,const char * func,const char * failedexpr)
{
  rsxgl_debug_printf("assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
		     failedexpr, file, line,
		     func ? ", function: " : "", func ? func : "");
  abort();
  /* NOTREACHED */
}

GLAPI void APIENTRY
glInitDebug(GLsizei maxbuf,void (* _debug_printf_callback)(GLsizei,const GLchar *))
{
  if(debug_buffer != 0) {
    free(debug_buffer);
    debug_buffer = 0;
  }

  ndebug_buffer = maxbuf;

  if(ndebug_buffer > 0) {
    debug_buffer = (char *)malloc(ndebug_buffer);
  }

  debug_printf_callback = _debug_printf_callback;
}
