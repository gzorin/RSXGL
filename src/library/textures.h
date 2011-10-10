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

enum rsxgl_texture_formats {
  RSXGL_TEX_FORMAT_B8 = 0,
  RSXGL_TEX_FORMAT_A1R5G5B5 = 1,
  RSXGL_TEX_FORMAT_A4R4G4B4 = 2,
  RSXGL_TEX_FORMAT_R5G6B5 = 3,
  RSXGL_TEX_FORMAT_A8R8G8B8 = 4,
  RSXGL_TEX_FORMAT_COMPRESSED_DXT1 = 5,
  RSXGL_TEX_FORMAT_COMPRESSED_DXT23 = 6,
  RSXGL_TEX_FORMAT_COMPRESSED_DXT45 = 7,
  RSXGL_TEX_FORMAT_G8B8 = 8,
  RSXGL_TEX_FORMAT_R6G5B5 = 9,
  RSXGL_TEX_FORMAT_DEPTH24_D8 = 10,
  RSXGL_TEX_FORMAT_DEPTH24_D8_FLOAT = 11,
  RSXGL_TEX_FORMAT_DEPTH16 = 12,
  RSXGL_TEX_FORMAT_DEPTH16_FLOAT = 13,
  RSXGL_TEX_FORMAT_X16 = 14,
  RSXGL_TEX_FORMAT_Y16_X16 = 15,
  RSXGL_TEX_FORMAT_R5G5B5A1 = 16,
  RSXGL_TEX_FORMAT_COMPRESSED_HILO8 = 17,
  RSXGL_TEX_FORMAT_COMPRESSED_HILO_S8 = 18,
  RSXGL_TEX_FORMAT_W16_Z16_Y16_X16_FLOAT = 19,
  RSXGL_TEX_FORMAT_W32_Z32_Y32_X32_FLOAT = 20,
  RSXGL_TEX_FORMAT_X32_FLOAT = 21,
  RSXGL_TEX_FORMAT_D1R5G5B5 = 22,
  RSXGL_TEX_FORMAT_D8R8G8B8 = 23,
  RSXGL_TEX_FORMAT_Y16_X16_FLOAT = 24,
  RSXGL_TEX_FORMAT_COMPRESSED_B8R8_G8R8 = 25,
  RSXGL_TEX_FORMAT_COMPRESSED_R8B8_R8G8 = 26,
  RSXGL_MAX_TEX_FORMATS = 27,

  RSXGL_TEX_FORMAT_SZ = 0,
  RSXGL_TEX_FORMAT_LN = 0x20,
  RSXGL_TEX_FORMAT_NR = 0,
  RSXGL_TEX_FORMAT_UN = 0x40
};

enum rsxgl_texture_remap_outputs {
  RSXGL_TEXTURE_REMAP_ZERO = 0,
  RSXGL_TEXTURE_REMAP_ONE = 1,
  RSXGL_TEXTURE_REMAP_REMAP = 2,

  // Unused by the RSX, but used internally:
  RSXGL_TEXTURE_REMAP_IGNORE = 3
};

enum rsxgl_texture_remap_inputs {
  RSXGL_TEXTURE_REMAP_FROM_A = 0,
  RSXGL_TEXTURE_REMAP_FROM_B = 1,
  RSXGL_TEXTURE_REMAP_FROM_G = 2,
  RSXGL_TEXTURE_REMAP_FROM_R = 3
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
    uint8_t invalid_contents:1, internalformat:5, dims:2;
    dimension_size_type size[3];
    void * data;
    memory_t memory;
    memory_arena_t::name_type arena;

    level_t();
    ~level_t();
  } levels[max_levels];

  uint16_t invalid:1,valid:1,immutable:1,internalformat:5, cube:1, rect:1, max_level:4, dims:2;

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

void rsxgl_texture_validate(rsxgl_context_t *,texture_t &,const uint32_t);
void rsxgl_textures_validate(rsxgl_context_t *,program_t &,const uint32_t);

#endif
