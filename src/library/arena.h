//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// arena.h - Private constants, types and functions related to memory arenas.

#ifndef rsxgl_arena_H
#define rsxgl_arena_H

#include "rsxgl_limits.h"
#include "mem.h"
#include "gl_object.h"
#include "gl_fifo.h"

#include <stddef.h>

#include <boost/integer.hpp>

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

  void * address;
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
  void * (*malloc_fn)(memory_arena_t *,rsx_size_t);
  void * (*memalign_fn)(memory_arena_t *,rsx_size_t,rsx_size_t);

  // resize (why?)
  void * (*realloc_fn)(memory_arena_t *,void *,rsx_size_t);

  // free
  void (*free_fn)(memory_arena_t *,void *);

  // address_to_offset
  int32_t (*address_to_offset_fn)(memory_arena_t *,void *,uint32_t *);

  // offset_to_address
  int32_t (*offset_to_address_fn)(memory_arena_t *,uint32_t,void * *);
#endif
};

memory_t rsxgl_arena_allocate(memory_arena_t &,rsx_size_t,rsx_size_t,void * * = 0);
void rsxgl_arena_free(memory_arena_t &,const memory_t &);

static inline void *
rsxgl_arena_address(memory_arena_t & arena,const memory_t & memory)
{
  return (uint8_t *)arena.address + (ptrdiff_t)(memory.offset - arena.memory.offset);
}

static inline memory_t
rsxgl_arena_memory(memory_arena_t & arena,void * address)
{
  return memory_t(arena.memory.location,arena.memory.offset + ((uint8_t *)address - (uint8_t *)(arena.address)),0);
}

static inline void
rsxgl_memory_transfer(gcmContextData * context,
		      const memory_t & dst,const int32_t dstpitch,const uint8_t dstbytes,
		      const memory_t & src,const int32_t srcpitch,const uint8_t srcbytes,
		      const uint32_t linelength,const uint32_t linecount)
{
  rsxgl_debug_printf("%s: dst:%u (%u) %u %u src:%u (%u) %u %u; %ux%u\n",
		     __PRETTY_FUNCTION__,
		     dst.offset,dst.location,dstpitch,dstbytes,
		     src.offset,src.location,srcpitch,srcbytes,
		     linelength,linecount);

  uint32_t * buffer = gcm_reserve(context,12);

  // NV_MEMORY_TO_MEMORY_FORMAT_DMA_BUFFER_IN = 0x184
  gcm_emit_channel_method_at(buffer,0,1,0x184,2);
  gcm_emit_at(buffer,1,src.location == RSXGL_MEMORY_LOCATION_MAIN ? RSXGL_DMA_MEMORY_HOST_BUFFER : RSXGL_DMA_MEMORY_FRAME_BUFFER);
  gcm_emit_at(buffer,2,dst.location == RSXGL_MEMORY_LOCATION_MAIN ? RSXGL_DMA_MEMORY_HOST_BUFFER : RSXGL_DMA_MEMORY_FRAME_BUFFER);

  // NV_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN = 0x30c
  gcm_emit_channel_method_at(buffer,3,1,0x30c,8);
  gcm_emit_at(buffer,4,src.offset);
  gcm_emit_at(buffer,5,dst.offset);
  gcm_emit_at(buffer,6,srcpitch);
  gcm_emit_at(buffer,7,dstpitch);
  gcm_emit_at(buffer,8,linelength);
  gcm_emit_at(buffer,9,linecount);
  gcm_emit_at(buffer,10,((uint32_t)dstbytes << 8) | ((uint32_t)srcbytes));
  gcm_emit_at(buffer,11,0);

  gcm_finish_n_commands(context,12);
}

#endif
