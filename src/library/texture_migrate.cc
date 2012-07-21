// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// texture_migrate.cc - manage the texture migration buffer

#include "texture_migrate.h"

#include "rsxgl_config.h"
#include "debug.h"
#include "rsxgl_assert.h"
#include "rsxgl_limits.h"

#include <rsx/gcm_sys.h>

#include <malloc.h>

// Size of migration buffer:
static uint32_t rsxgl_texture_migrate_size = RSXGL_CONFIG_texture_migrate_buffer_size, rsxgl_texture_migrate_align = RSXGL_TEXTURE_MIGRATE_BUFFER_ALIGN;

// 
static void * _rsxgl_texture_migrate_buffer = 0;
static uint32_t rsxgl_texture_migrate_buffer_offset = 0;
static mspace rsxgl_texture_migrate_buffer_space = 0;

static inline
void * rsxgl_texture_migrate_buffer()
{
  if(_rsxgl_texture_migrate_buffer == 0) {
    //
#if ((RSXGL_TEXTURE_MIGRATE_BUFFER_LOCATION) == RSXGL_MEMORY_LOCATION_LOCAL)
    _rsxgl_texture_migrate_buffer = rsxgl_rsx_memalign(rsxgl_texture_migrate_align,rsxgl_texture_migrate_size);
    if(_rsxgl_texture_migrate_buffer == 0) {
      __rsxgl_assert_func(__FILE__,__LINE__,__PRETTY_FUNCTION__,"failed to allocate texture migration buffer in RSX memory");
    }

    int32_t s = gcmAddressToOffset(_rsxgl_texture_migrate_buffer,rsxgl_texture_migrate_size,&rsxgl_texture_migrate_buffer_offset);
    if(s != 0) {
      __rsxgl_assert_func(__FILE__,__LINE__,__PRETTY_FUNCTION__,"failed to compute offset for texture migration buffer");
    }
#elif ((RSXGL_TEXTURE_MIGRATE_BUFFER_LOCATION) == RSXGL_MEMORY_LOCATION_MAIN)
    _rsxgl_texture_migrate_buffer = memalign(rsxgl_texture_migrate_align,rsxgl_texture_migrate_size);
    if(_rsxgl_texture_migrate_buffer == 0) {
      __rsxgl_assert_func(__FILE__,__LINE__,__PRETTY_FUNCTION__,"failed to allocate texture migration buffer in main memory");
    }

    int32_t s = gcmMapMainMemory(_rsxgl_texture_migrate_buffer,rsxgl_texture_migrate_size,&rsxgl_texture_migrate_buffer_offset);
    if(s != 0) {
      __rsxgl_assert_func(__FILE__,__LINE__,__PRETTY_FUNCTION__,"failed to map texture migration buffer into RSX memory");
    }
#else
    rsxgl_assert(0);
#endif

    rsxgl_assert(_rsxgl_texture_migrate_buffer != 0);

    rsxgl_texture_migrate_buffer_space = create_mspace_with_base(_rsxgl_texture_migrate_buffer,rsxgl_texture_migrate_size,0);
  }

  return _rsxgl_texture_migrate_buffer;
}

void *
rsxgl_texture_migrate_memalign(const rsx_size_t align,const rsx_size_t size)
{
  void * buffer = rsxgl_texture_migrate_buffer();

  rsxgl_assert(buffer != 0);

  return mspace_memalign(rsxgl_texture_migrate_buffer_space,align,size);
}

void
rsxgl_texture_migrate_free(void * ptr)
{
  rsxgl_assert(_rsxgl_texture_migrate_buffer != 0);

  mspace_free(rsxgl_texture_migrate_buffer_space,ptr);
}

void
rsxgl_texture_migrate_reset()
{
}

void *
rsxgl_texture_migrate_address(const uint32_t offset)
{
  rsxgl_assert(_rsxgl_texture_migrate_buffer != 0);
  return (uint8_t *)_rsxgl_texture_migrate_buffer + (ptrdiff_t)(offset - rsxgl_texture_migrate_buffer_offset);
}

uint32_t
rsxgl_texture_migrate_offset(const void * ptr)
{
  rsxgl_assert(_rsxgl_texture_migrate_buffer != 0);
  return rsxgl_texture_migrate_buffer_offset + ((uint8_t *)ptr - (uint8_t *)_rsxgl_texture_migrate_buffer);
}

void *
rsxgl_texture_migrate_base()
{
  rsxgl_assert(_rsxgl_texture_migrate_buffer != 0);
  return (uint8_t *)_rsxgl_texture_migrate_buffer;
}
