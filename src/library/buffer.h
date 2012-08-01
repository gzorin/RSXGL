//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// buffer.h - Private constants, types and functions related to memory buffers.

#ifndef rsxgl_buffer_H
#define rsxgl_buffer_H

#include "gl_constants.h"
#include "rsxgl_limits.h"
#include "gl_object.h"
#include "arena.h"

enum rsxgl_buffer_target {
  RSXGL_ARRAY_BUFFER = 0,
  RSXGL_COPY_READ_BUFFER = 1,
  RSXGL_COPY_WRITE_BUFFER = 2,
  RSXGL_ELEMENT_ARRAY_BUFFER = 3,
  RSXGL_PIXEL_PACK_BUFFER = 4,
  RSXGL_PIXEL_UNPACK_BUFFER = 5,
  RSXGL_TEXTURE_BUFFER = 6,
  RSXGL_TRANSFORM_FEEDBACK_BUFFER = 7,
  RSXGL_UNIFORM_BUFFER = 8,
  RSXGL_TRANSFORM_FEEDBACK_BUFFER0 = RSXGL_UNIFORM_BUFFER + 1,
  RSXGL_UNIFORM_BUFFER0 = RSXGL_TRANSFORM_FEEDBACK_BUFFER0 + RSXGL_MAX_TRANSFORM_FEEDBACK_BUFFER_BINDINGS,
  RSXGL_MAX_BUFFER_TARGETS = RSXGL_UNIFORM_BUFFER0 + RSXGL_MAX_UNIFORM_BUFFER_BINDINGS
};

enum rsxgl_buffer_range_target {
  RSXGL_TRANSFORM_FEEDBACK_BUFFER_RANGE0 = 0,
  RSXGL_UNIFORM_BUFFER_RANGE0 = RSXGL_TRANSFORM_FEEDBACK_BUFFER_RANGE0 + RSXGL_MAX_TRANSFORM_FEEDBACK_BUFFER_BINDINGS,
  RSXGL_MAX_BUFFER_RANGE_TARGETS = RSXGL_UNIFORM_BUFFER_RANGE0 + RSXGL_MAX_UNIFORM_BUFFER_BINDINGS
};

enum rsxgl_buffer_access {
  RSXGL_READ_ONLY = 1,
  RSXGL_WRITE_ONLY = 2,
  RSXGL_READ_WRITE = 3
};

enum rsxgl_buffer_usage {
  RSXGL_STREAM_DRAW = 0,
  RSXGL_STREAM_READ = 1,
  RSXGL_STREAM_COPY = 2,
  RSXGL_STATIC_DRAW = 3,
  RSXGL_STATIC_READ = 4,
  RSXGL_STATIC_COPY = 5,
  RSXGL_DYNAMIC_DRAW = 6,
  RSXGL_DYNAMIC_READ = 7,
  RSXGL_DYNAMIC_COPY = 8
};

struct buffer_t {
  typedef bindable_gl_object< buffer_t, RSXGL_MAX_BUFFERS, RSXGL_MAX_BUFFER_TARGETS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  uint32_t deleted:1, timestamp:31;
  uint32_t ref_count;

  uint8_t invalid:1,usage:4,mapped:2;

  memory_t memory;
  memory_arena_t::name_type arena;
  rsx_size_t size;

  rsx_size_t mapped_offset, mapped_size;

  buffer_t()
    : deleted(0), timestamp(0), ref_count(0), invalid(0), usage(0), mapped(0), arena(0), size(0), mapped_offset(0), mapped_size(0) {
  }

  ~buffer_t();
};

static inline uint32_t
rsxgl_pointer_to_offset(const void * ptr)
{
  return (uint32_t)((uint64_t)ptr);
}

struct rsxgl_context_t;

void rsxgl_buffer_validate(rsxgl_context_t *,buffer_t &,const uint32_t,const uint32_t,const uint32_t);

#endif
