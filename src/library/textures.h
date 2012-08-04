//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// textures.h - Private constants, types and functions related to texture maps.

#ifndef rsxgl_textures_H
#define rsxgl_textures_H

#include "gl_constants.h"
#include "rsxgl_limits.h"
#include "gl_object.h"
#include "arena.h"
#include "program.h"

#include "pipe/p_format.h"

#include <boost/integer/static_log2.hpp>

// These numbers correspond to what the RSX hardware expects. They are different from the numbers
// associated with the same enum's for the depth test, so they can't be shared with that.
enum rsxgl_texture_compare_funcs {
  RSXGL_TEXTURE_COMPARE_NEVER = 0,
  RSXGL_TEXTURE_COMPARE_GREATER = 1,
  RSXGL_TEXTURE_COMPARE_EQUAL = 2,
  RSXGL_TEXTURE_COMPARE_GEQUAL = 3,
  RSXGL_TEXTURE_COMPARE_LESS = 4,
  RSXGL_TEXTURE_COMPARE_NOTEQUAL = 5,
  RSXGL_TEXTURE_COMPARE_LEQUAL = 6,
  RSXGL_TEXTURE_COMPARE_ALWAYS = 7
};

enum rsxgl_sampler_wrap_modes {
  RSXGL_REPEAT = 0,
  RSXGL_CLAMP_TO_EDGE = 1,
  RSXGL_MIRRORED_REPEAT = 2
};

enum rsxgl_sampler_filter_modes {
  RSXGL_NEAREST = 0,
  RSXGL_LINEAR = 1,
  RSXGL_NEAREST_MIPMAP_NEAREST = 2,
  RSXGL_LINEAR_MIPMAP_NEAREST = 3,
  RSXGL_NEAREST_MIPMAP_LINEAR = 4,
  RSXGL_LINEAR_MIPMAP_LINEAR = 5
};

struct sampler_t {
  typedef bindable_gl_object< sampler_t, RSXGL_MAX_SAMPLERS, RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  uint16_t wrap_s:2, wrap_t:2, wrap_r:2,
    filter_min:3, filter_mag:3,
    compare_mode:1, compare_func:4;

  float lodBias, minLod, maxLod;

  sampler_t();
  void destroy() {}
};

enum rsxgl_texture_swizzle_inputs {
  RSXGL_TEXTURE_SWIZZLE_FROM_R = 1,
  RSXGL_TEXTURE_SWIZZLE_FROM_G = 2,
  RSXGL_TEXTURE_SWIZZLE_FROM_B = 3,
  RSXGL_TEXTURE_SWIZZLE_FROM_A = 0,
  RSXGL_TEXTURE_SWIZZLE_ZERO = 4,
  RSXGL_TEXTURE_SWIZZLE_ONE = 5
};

struct texture_t {
  typedef bindable_gl_object< texture_t, RSXGL_MAX_TEXTURES, RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, 1 > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  typedef object_binding_type< buffer_t, RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS > buffer_binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  uint32_t deleted:1, timestamp:31;
  uint32_t ref_count;

  texture_t();
  ~texture_t();

  static const boost::static_log2_argument_type max_levels = boost::static_log2< RSXGL_MAX_TEXTURE_SIZE >::value + 1;
  typedef boost::uint_value_t< max_levels - 1 >::least level_size_type;

  typedef boost::uint_value_t< RSXGL_MAX_TEXTURE_SIZE - 1 >::least dimension_size_type;

  // --- Cold:
  struct level_t {
    uint8_t dims:2, cube:1, rect:1;
    pipe_format pformat;
    dimension_size_type size[3];
    uint32_t pitch;
    memory_t memory;

    level_t();
    ~level_t();
  } levels[max_levels];

  uint16_t invalid:1, invalid_complete:1,
    complete:1, immutable:1,
    dims:2, cube:1, rect:1,
    num_levels:4;

  struct {
    uint16_t r:3, g:3, b:3, a:3;
  } swizzle;
  
  pipe_format pformat;

  // --- Hot:
  uint32_t format;
  dimension_size_type size[3], pad;
  uint32_t pitch;
  uint32_t remap;

  memory_t memory;
  memory_arena_t::name_type arena;

  sampler_t sampler;
};

struct rsxgl_context_t;

bool rsxgl_texture_validate_complete(rsxgl_context_t *,texture_t &);
void rsxgl_texture_validate(rsxgl_context_t *,texture_t &,uint32_t);
void rsxgl_textures_validate(rsxgl_context_t *,program_t &,uint32_t);

#endif
