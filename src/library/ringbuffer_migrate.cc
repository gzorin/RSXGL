// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// ringbuffer_migrate.cc

#include "rsxgl_config.h"

#include "debug.h"
#include "rsxgl_assert.h"
#include "rsxgl_limits.h"
#include "mem.h"
#include "sync.h"
#include "migrate.h"

// Size of migration buffer:
static uint32_t rsxgl_migrate_size = RSXGL_CONFIG_migrate_buffer_size, rsxgl_migrate_align = RSXGL_MIGRATE_BUFFER_ALIGN;

// 
static rsx_ptr_t _rsxgl_migrate_buffer = 0;
static uint8_t rsxgl_migrate_sync = 0;

// Head and tail position of the buffer:
static uint32_t rsxgl_migrate_head = 0, rsxgl_migrate_tail = 0;

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
    _rsxgl_migrate_buffer = rsx_memalign(rsxgl_migrate_align,rsxgl_migrate_size);
    rsxgl_assert(_rsxgl_migrate_buffer != 0);
#elif (RSXGL_MIGRATE_BUFFER_LOCATION == RSXGL_MEMORY_LOCATION_MAIN)
    rsxgl_assert(0);
#else
    rsxgl_assert(0);
#endif

    // 
    rsxgl_migrate_sync = rsxgl_sync_object_allocate();
    rsxgl_assert(rsxgl_migrate_sync != 0);

    rsxgl_sync_cpu_signal(rsxgl_migrate_sync,0);
  }

  return _rsxgl_migrate_buffer;
}

rsx_ptr_t
rsxgl_ringbuffer_migrate_memalign(gcmContextData *,const rsx_size_t align,const rsx_size_t size)
{
  rsx_ptr_t buffer = rsxgl_migrate_buffer();

  rsxgl_assert(buffer != 0);
  rsxgl_assert(rsxgl_migrate_sync != 0);

#if !defined(NDEBUG)
  rsxgl_assert(rsxgl_migrate_stack == 0);
  rsxgl_migrate_stack = 1;
#endif

  // TODO - Grow the ringbuffer
  if(size > rsxgl_migrate_size) {
    __rsxgl_assert_func(__FILE__,__LINE__,__PRETTY_FUNCTION__,"ringbuffer_migrate buffer is full, and growing it is currently unimplemented");
  }

  const uint32_t tail_mod_align = rsxgl_migrate_tail & (align - 1);
  const uint32_t offset = (tail_mod_align != 0) ? (rsxgl_migrate_tail + align - tail_mod_align) : rsxgl_migrate_tail;
  const uint32_t new_tail = offset + size;

  const bool
    wrap = new_tail > rsxgl_migrate_size,
    bump = (offset < rsxgl_migrate_head) && (new_tail > rsxgl_migrate_head);

  if(wrap || bump) {
    volatile uint32_t * phead = gcmGetLabelAddress(rsxgl_migrate_sync);
    rsxgl_assert(phead != 0);

    // TODO - see if an actual mutex is needed here:
    uint32_t head = *phead;
    while((wrap) ? (head < size) : (head < new_tail) && (head != rsxgl_migrate_tail)) {
      usleep(RSXGL_SYNC_SLEEP_INTERVAL);
      head = *phead;
    }

    if(head == rsxgl_migrate_tail) {
      rsxgl_migrate_head = 0;
      rsxgl_migrate_tail = size;

      return buffer;
    }
    else {
      rsxgl_migrate_head = head;
      rsxgl_migrate_tail = (bump) ? new_tail : size;

      return (uint8_t *)buffer + (wrap ? 0 : offset);
    }
  }
  else {
    rsxgl_migrate_tail = new_tail;
    return (uint8_t *)buffer + offset;
  }
}

void
rsxgl_ringbuffer_migrate_free(gcmContextData * context,const rsx_ptr_t ptr,const rsx_size_t size)
{
  rsxgl_assert(_rsxgl_migrate_buffer != 0);
  rsxgl_assert(rsxgl_migrate_sync != 0);

#if !defined(NDEBUG)
  rsxgl_assert(rsxgl_migrate_stack == 1);
  rsxgl_migrate_stack = 0;
#endif

  // TODO - see if an actual mutex is needed here:
  const uint32_t offset = (uint8_t *)ptr - (uint8_t *)_rsxgl_migrate_buffer + size;

  rsxgl_emit_sync_gpu_signal_read(context,rsxgl_migrate_sync,offset);
}

void
rsxgl_ringbuffer_migrate_reset(gcmContextData * context)
{
  if(rsxgl_migrate_sync != 0) {
    rsxgl_migrate_head = 0;
    rsxgl_migrate_tail = 0;

    volatile uint32_t * phead = gcmGetLabelAddress(rsxgl_migrate_sync);
    rsxgl_assert(phead != 0);
    *phead = 0;
  }
}
