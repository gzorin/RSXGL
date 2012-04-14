#include <EGL/egl.h>
#include "GL3/rsxgl.h"
#include "debug.h"
#include "mem.h"

#include <rsx/gcm_sys.h>

#define MSPACES 1
#define ONLY_MSPACES 1
#define HAVE_MMAP 0
#define malloc_getpagesize 4096
#include "malloc-2.8.4.h"
#undef MSPACES
#undef ONLY_MSPACES
#undef HAVE_MMAP
#undef malloc_getpagesize

#include <assert.h>

//uint32_t rsxgl_rsx_mspace_offset = 0, rsxgl_rsx_mspace_size = 0;

extern struct rsxgl_init_parameters_t rsxgl_init_parameters;

mspace
rsxgl_rsx_mspace()
{
  static mspace _rsx_mspace = 0;

  if(_rsx_mspace == 0) {
    gcmConfiguration config;
    gcmGetConfiguration(&config);

    const uint32_t offset = rsxgl_init_parameters.rsx_mspace_offset;
    const uint32_t available = config.localSize - offset;
    const uint32_t size = (rsxgl_init_parameters.rsx_mspace_size == 0 || rsxgl_init_parameters.rsx_mspace_size > available) ? available : rsxgl_init_parameters.rsx_mspace_size;

    rsxgl_debug_printf("%s: want %u bytes (of available %u) starting at offset %u (%lu)\n",
		       __PRETTY_FUNCTION__,
		       size,available,offset,(uint64_t)config.localAddress + offset);

    _rsx_mspace = create_mspace_with_base((uint8_t *)config.localAddress + offset,size,0);
  }

  assert(_rsx_mspace != 0);

  return _rsx_mspace;
}

void *
rsxgl_rsx_malloc(rsx_size_t size)
{  
  return mspace_malloc(rsxgl_rsx_mspace(),size);
}

void *
rsxgl_rsx_memalign(rsx_size_t alignment,rsx_size_t size)
{
  return mspace_memalign(rsxgl_rsx_mspace(),alignment,size);
}

void *
rsxgl_rsx_realloc(void * mem,rsx_size_t size)
{
  return mspace_realloc(rsxgl_rsx_mspace(),mem,size);
}

void
rsxgl_rsx_free(void * mem)
{
  mspace_free(rsxgl_rsx_mspace(),mem);
}
