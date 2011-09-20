//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// arena.h - Private constants, types and functions related to memory arenas.

#ifndef rsxgl_arena_H
#define rsxgl_arena_H

#include "rsxgl_limits.h"
#include "gcm.h"
#include "mem.h"
#include "gl_object.h"

#include <stddef.h>

#include <boost/integer.hpp>

enum memory_location {
  RSXGL_MEMORY_LOCATION_LOCAL = 0,
  RSXGL_MEMORY_LOCATION_MAIN = 1
};

enum rsxgl_arena_target {
  RSXGL_BUFFER_ARENA = 0,
  RSXGL_TEXTURE_ARENA = 1,
  RSXGL_RENDERBUFFER_ARENA = 2,
  RSXGL_MAX_ARENA_TARGETS = 3
};

enum rsxgl_transfer_location {
  RSXGL_DMA_MEMORY_FRAME_BUFFER = 0xFEED0000,
  RSXGL_DMA_MEMORY_HOST_BUFFER = 0xFEED0001
};

#define RSXGL_TRANSFER_LOCATION(location) (((location) == RSXGL_MEMORY_LOCATION_LOCAL) ? RSXGL_DMA_MEMORY_FRAME_BUFFER : RSXGL_DMA_MEMORY_HOST_BUFFER)

struct memory_t {
  uint32_t location:1, offset:30, owner:1;
  
  memory_t()
    : location(0), offset(0), owner(0) {
  }
  
  memory_t(uint32_t _location,uint32_t _offset,uint32_t _owner = 0)
    : location(_location), offset(_offset), owner(_owner) {
  }

  memory_t operator +(uint32_t incr) const {
    return memory_t(location,offset + incr,owner);
  }
  
  memory_t & operator +=(uint32_t incr) {
    offset += incr;
    return *this;
  }

  operator bool() const {
    return offset != 0;
  }

  bool operator ==(uint32_t rhs) const {
    return offset == rhs;
  }

  bool operator !=(uint32_t rhs) const {
    return offset != rhs;
  }
};

struct memory_arena_t;

struct memory_arena_t {
  typedef bindable_gl_object< memory_arena_t, RSXGL_MAX_ARENAS, RSXGL_MAX_ARENA_TARGETS, 1 > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  rsx_ptr_t address;
  mspace space;
  memory_t memory;
  rsx_size_t size;

  memory_arena_t()
    : address(0), space(0), size(0) {
  }

  void destroy();

#if 0
  // eventually:
  void * user_data;

  // functions:
  // allocate
  rsx_ptr_t (*malloc_fn)(memory_arena_t *,rsx_size_t);
  rsx_ptr_t (*memalign_fn)(memory_arena_t *,rsx_size_t,rsx_size_t);

  // resize (why?)
  rsx_ptr_t (*realloc_fn)(memory_arena_t *,rsx_ptr_t,rsx_size_t);

  // free
  void (*free_fn)(memory_arena_t *,rsx_ptr_t);

  // address_to_offset
  int32_t (*address_to_offset_fn)(memory_arena_t *,rsx_ptr_t,uint32_t *);

  // offset_to_address
  int32_t (*offset_to_address_fn)(memory_arena_t *,uint32_t,rsx_ptr_t *);
#endif
};

memory_t rsxgl_arena_allocate(memory_arena_t &,rsx_size_t,rsx_size_t,rsx_ptr_t * = 0);
void rsxgl_arena_free(memory_arena_t &,const memory_t &);

static inline rsx_ptr_t
rsxgl_arena_address(memory_arena_t & arena,const memory_t & memory)
{
  return (uint8_t *)arena.address + (ptrdiff_t)(memory.offset - arena.memory.offset);
}

static inline memory_t
rsxgl_arena_memory(memory_arena_t & arena,rsx_ptr_t address)
{
  return memory_t(arena.memory.location,arena.memory.offset + ((uint8_t *)address - (uint8_t *)(arena.address)),0);
}

#endif
