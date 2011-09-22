/* $Id: realpath.h,v 1.2 2001/02/09 01:55:36 djm Exp $ */

#ifndef _BSD_REALPATH_H
#define _BSD_REALPATH_H

//#include "config.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(HAVE_REALPATH) || defined(BROKEN_REALPATH)

char *realpath(const char *path, char *resolved);

#endif /* !defined(HAVE_REALPATH) || defined(BROKEN_REALPATH) */

#if defined(__cplusplus)
};
#endif

#endif /* _BSD_REALPATH_H */
