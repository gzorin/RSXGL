//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// framebuffer.h - Offscreen rendering targets.

#ifndef rsxgl_framebuffer_H
#define rsxgl_framebuffer_H

#include "gl_constants.h"
#include "rsxgl_limits.h"
#include "gl_object.h"
#include "arena.h"

enum rsxgl_renderbuffer_target {
  RSXGL_RENDERBUFFER = 0,
  RSXGL_MAX_RENDERBUFFER_TARGETS = 1
};

enum rsxgl_renderbuffer_formats {
  RSXGL_RENDERBUFFER_FORMAT_R5G6B5 = 0,
  RSXGL_RENDERBUFFER_FORMAT_X8R8G8B8 = 1,
  RSXGL_RENDERBUFFER_FORMAT_A8R8G8B8 = 2,
  RSXGL_RENDERBUFFER_FORMAT_B8 = 3,
  RSXGL_RENDERBUFFER_FORMAT_A16B16G16R16_FLOAT = 4,
  RSXGL_RENDERBUFFER_FORMAT_A32B32G32R32_FLOAT = 5,
  RSXGL_RENDERBUFFER_FORMAT_R32_FLOAT = 6,
  RSXGL_RENDERBUFFER_FORMAT_X8B8G8R8 = 7,
  RSXGL_RENDERBUFFER_FORMAT_A8B8G8R8 = 8,
  RSXGL_RENDERBUFFER_FORMAT_DEPTH24_D8 = 9,
  RSXGL_RENDERBUFFER_FORMAT_DEPTH16 = 10
};

struct surface_t {
  typedef boost::uint_value_t< RSXGL_MAX_RENDERBUFFER_SIZE - 1 >::least dimension_size_type;

  uint8_t format;
  dimension_size_type size[2];
  uint32_t pitch;
  memory_t memory;

  surface_t()
    : format(~0), pitch(0) {
    size[0] = 0;
    size[1] = 0;
  }
};

struct renderbuffer_t {
  typedef bindable_gl_object< renderbuffer_t, RSXGL_MAX_RENDERBUFFERS, RSXGL_MAX_RENDERBUFFER_TARGETS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  memory_arena_t::name_type arena;
  surface_t surface;

  renderbuffer_t();
  ~renderbuffer_t();
};

enum rsxgl_framebuffer_target {
  RSXGL_DRAW_FRAMEBUFFER = 0,
  RSXGL_READ_FRAMEBUFFER = 1,
  RSXGL_MAX_FRAMEBUFFER_TARGETS = 2
};

struct framebuffer_t {
  typedef bindable_gl_object< framebuffer_t, RSXGL_MAX_FRAMEBUFFERS, RSXGL_MAX_FRAMEBUFFER_TARGETS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

#if 0

  // attachments - 4 color, 1 depth + stencil (up to 5)
  // these have a type, too - renderbuffer or texture
  // there is also a mapping (set by glDrawBuffer() or glDrawBuffers()) - up to 4 values

#endif  

  framebuffer_t();
  ~framebuffer_t();
};

struct rsxgl_context_t;

void rsxgl_draw_framebuffer_validate(rsxgl_context_t *);

#endif
