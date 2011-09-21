#ifndef rsxgl_mem_H
#define rsxgl_mem_H

#include <gcm.h>

#define MSPACES 1
#define ONLY_MSPACES 1
#define HAVE_MMAP 0
#define malloc_getpagesize 4096
#include "malloc-2.8.4.h"
#undef MSPACES
#undef ONLY_MSPACES
#undef HAVE_MMAP
#undef malloc_getpagesize

#ifdef __cplusplus
extern "C" {
#endif

rsx_ptr_t rsxgl_rsx_malloc(rsx_size_t);
rsx_ptr_t rsxgl_rsx_memalign(rsx_size_t,rsx_size_t);
rsx_ptr_t rsxgl_rsx_realloc(rsx_ptr_t,rsx_size_t);
void rsxgl_rsx_free(rsx_ptr_t);

#ifdef __cplusplus
}
#endif

#endif
