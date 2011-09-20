#include "debug.h"
#include "mem.h"

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

mspace
rsx_mspace()
{
  static mspace _rsx_mspace = 0;

  if(_rsx_mspace == 0) {
    gcmConfiguration config;
    gcmGetConfiguration(&config);

    //rsxgl_debug_printf("%s: %lu %lu, %lu %lu\n",__PRETTY_FUNCTION__,config.localAddress,(unsigned long)config.localSize,config.ioAddress,(unsigned long)config.ioSize);

    _rsx_mspace = create_mspace_with_base(config.localAddress,config.localSize,0);
  }

  assert(_rsx_mspace != 0);

  return _rsx_mspace;
}

rsx_ptr_t
rsx_malloc(rsx_size_t size)
{  
  return mspace_malloc(rsx_mspace(),size);
}

rsx_ptr_t
rsx_memalign(rsx_size_t alignment,rsx_size_t size)
{
  return mspace_memalign(rsx_mspace(),alignment,size);
}

rsx_ptr_t
rsx_realloc(rsx_ptr_t mem,rsx_size_t size)
{
  return mspace_realloc(rsx_mspace(),mem,size);
}

void
rsx_free(rsx_ptr_t mem)
{
  mspace_free(rsx_mspace(),mem);
}
