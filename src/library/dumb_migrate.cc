// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// dumb_migrate.cc

#include "rsxgl_config.h"

#include "debug.h"
#include "rsxgl_assert.h"
#include "rsxgl_limits.h"
#include "mem.h"
#include "migrate.h"

// Size of migration buffer:
static uint32_t rsxgl_migrate_size = RSXGL_CONFIG_migrate_buffer_size, rsxgl_migrate_align = RSXGL_MIGRATE_BUFFER_ALIGN;

// 
static rsx_ptr_t _rsxgl_migrate_buffer = 0;

// Tail position - nothing else:
static uint32_t rsxgl_migrate_tail = 0;

// memalign/free calls do not stack - this is here to ensure that
#if !defined(NDEBUG)
static uint32_t rsxgl_migrate_stack = 0;
#endif

static inline
rsx_ptr_t rsxgl_migrate_buffer()
{
  if(_rsxgl_migrate_buffer == 0) {
    //
#if (RSXGL_MIGRATE_BUFFER_LOCATION == RSXGL_MEMORY_LOCATION_LOCAL)
    _rsxgl_migrate_buffer = rsxgl_rsx_memalign(rsxgl_migrate_align,rsxgl_migrate_size);
    rsxgl_assert(_rsxgl_migrate_buffer != 0);
#elif (RSXGL_MIGRATE_BUFFER_LOCATION == RSXGL_MEMORY_LOCATION_MAIN)
    rsxgl_assert(0);
#else
    rsxgl_assert(0);
#endif
  }

  return _rsxgl_migrate_buffer;
}

rsx_ptr_t
rsxgl_dumb_migrate_memalign(gcmContextData *,const rsx_size_t align,const rsx_size_t size)
{
  rsx_ptr_t buffer = rsxgl_migrate_buffer();

  rsxgl_assert(buffer != 0);

#if !defined(NDEBUG)
  rsxgl_assert(rsxgl_migrate_stack == 0);
  rsxgl_migrate_stack = 1;
#endif

  const uint32_t tail_mod_align = rsxgl_migrate_tail & (align - 1);
  const uint32_t offset = (tail_mod_align != 0) ? (rsxgl_migrate_tail + align - tail_mod_align) : rsxgl_migrate_tail;
  const uint32_t new_tail = offset + size;

  if(new_tail > rsxgl_migrate_size) {
    __rsxgl_assert_func(__FILE__,__LINE__,__PRETTY_FUNCTION__,"dumb_migrate buffer is full");
  }
  else {
    rsxgl_migrate_tail = new_tail;
    return (uint8_t *)buffer + offset;
  }
}

void
rsxgl_dumb_migrate_free(gcmContextData *,const_rsx_ptr_t ptr,const rsx_size_t size)
{
  rsxgl_assert(_rsxgl_migrate_buffer != 0);

#if !defined(NDEBUG)
  rsxgl_assert(rsxgl_migrate_stack == 1);
  rsxgl_migrate_stack = 0;
#endif
}

void
rsxgl_dumb_migrate_reset(gcmContextData *)
{
  rsxgl_migrate_tail = 0;
}
