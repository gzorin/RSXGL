//-*-C-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// debug.h - Facility for the library to report debugging information.

#ifndef rsxgl_debug_H
#define rsxgl_debug_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void rsxgl_debug_printf(const char *,...);
void rsxgl_debug_vprintf(const char *,va_list);

#ifdef __cplusplus
}
#endif

#endif
