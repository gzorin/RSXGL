#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <memory.h>
#include <windows.h>
#endif

/**
 * Instruction precision for GL_NV_fragment_program
 */
/*@{*/
#define FLOAT32  0x0
#define FLOAT16  0x1
#define FIXED12  0x2
/*@}*/

typedef signed char					s8;
typedef signed short				s16;
typedef signed int					s32;

#if _MSC_VER>1300
typedef signed __int64				s64;
#else
typedef signed long long			s64;
#endif

typedef unsigned char				u8;
typedef unsigned short				u16;
typedef unsigned int				u32;

#if _MSC_VER>1300
typedef unsigned __int64			u64;
#else
typedef unsigned long long			u64;
#endif

typedef float						f32;
typedef double						f64;

#ifndef FALSE
#define FALSE						0
#endif

#ifndef TRUE
#define TRUE						1
#endif

#ifndef INLINE
#define INLINE						__inline
#endif

#ifndef boolean
#define boolean						char
#endif

#define MIN2(a,b)		((a)<(b) ? (a) : (b))
#define MAX2(a,b)		((a)>(b) ? (a) : (b))

#if _MSC_VER>1300
#define strnicmp		_strnicmp
#define stricmp			_stricmp
#endif

#if defined(_MSC_VER)
#define strcasecmp		stricmp
#define strncasecmp		strnicmp
#elif defined(__GNUC__)
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#include "nv40prog.h"

#endif
