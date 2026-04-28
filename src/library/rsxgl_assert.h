//-*-C-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// rsxgl_assert.h - Variation on the C assert() macro that passes messages to rsxgl_debug_printf:

#ifndef rsxgl_assert_H
#define rsxgl_assert_H

#include <assert.h>
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NDEBUG
# define rsxgl_assert(__e) ((void)0)
#else
# define rsxgl_assert(__e) ((__e) ? (void)0 : __rsxgl_assert_func (__FILE__, __LINE__, \
							     __RSXGL_ASSERT_FUNC, #__e))

# ifndef __RSXGL_ASSERT_FUNC
  /* Use g++'s demangled names in C++.  */
#  if defined __cplusplus && defined __GNUC__
#   define __RSXGL_ASSERT_FUNC __PRETTY_FUNCTION__

  /* C99 requires the use of __func__.  */
#  elif __STDC_VERSION__ >= 199901L
#   define __RSXGL_ASSERT_FUNC __func__

  /* Older versions of gcc don't have __func__ but can use __FUNCTION__.  */
#  elif __GNUC__ >= 2
#   define __RSXGL_ASSERT_FUNC __FUNCTION__

  /* failed to detect __func__ support.  */
#  else
#   define __RSXGL_ASSERT_FUNC ((char *) 0)
#  endif
# endif /* !__RSXGL_ASSERT_FUNC */
#endif /* !NDEBUG */

#ifndef _EXFUN
# define _EXFUN(N,P) N P
#endif

void _EXFUN(__rsxgl_assert_func, (const char *, int, const char *, const char *)
	    _ATTRIBUTE ((__noreturn__)));

#ifdef __cplusplus
}
#endif

#endif
