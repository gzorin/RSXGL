//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// program.h - Private constants, types and functions related to shaders and GPU programs.

#ifndef rsxgl_program_H
#define rsxgl_program_H

#include "gl_constants.h"
#include "gl_object_storage.h"
#include "ieee32_t.h"
#include "compiler_context.h"

#include <memory>
#include <string>
#include <cstddef>
#include <cassert>
#include <vector>
#include <utility>

#include <boost/integer.hpp>
#include <boost/container/flat_set.hpp>

enum rsxgl_program_target {
  RSXGL_ACTIVE_PROGRAM = 0,
  RSXGL_MAX_PROGRAM_TARGETS = 1
};

enum rsxgl_shader_types {
  RSXGL_VERTEX_SHADER = 0,
  RSXGL_FRAGMENT_SHADER = 1,
  RSXGL_MAX_SHADER_TYPES = 2
};

enum rsxgl_data_types {
  RSXGL_DATA_TYPE_FLOAT = 0,
  RSXGL_DATA_TYPE_FLOAT2 = 1,
  RSXGL_DATA_TYPE_FLOAT3 = 2,
  RSXGL_DATA_TYPE_FLOAT4 = 3,
  RSXGL_DATA_TYPE_FLOAT4x4 = 4,
  RSXGL_DATA_TYPE_SAMPLER1D = 5,
  RSXGL_DATA_TYPE_SAMPLER2D = 6,
  RSXGL_DATA_TYPE_SAMPLER3D = 7,
  RSXGL_DATA_TYPE_SAMPLERCUBE = 8,
  RSXGL_DATA_TYPE_SAMPLERRECT = 9,
  RSXGL_MAX_DATA_TYPES = 10,
  RSXGL_DATA_TYPE_UNKNOWN = 10
};

struct shader_t {
  typedef gl_object< shader_t, RSXGL_MAX_SHADERS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;

  static storage_type & storage();

  shader_t();
  ~shader_t();

  // --- cold:
  uint32_t type:2,compiled:1,deleted:1,ref_count:28;

  std::string source;
  std::unique_ptr< uint8_t[] > binary;
  std::string info;

  gl_shader * mesa_shader;
};

struct program_t {
  typedef bindable_gl_object< program_t, RSXGL_MAX_PROGRAMS, RSXGL_MAX_PROGRAM_TARGETS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  //typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  //typedef typename gl_object_type::binding_type binding_type;
  typedef object_container_type< program_t, 1 > binding_type;

  static storage_type & storage();

  program_t();
  ~program_t();

  // --- cold:
  //
  uint32_t deleted:1,timestamp:31;

  uint32_t linked:1,validated:1,invalid_uniforms:1,ref_count:28;

  boost::container::flat_set< shader_t::name_type > attached_shaders, linked_shaders;

  // Information returned from glLinkProgram():
  std::string info;

  // Accumulate all of the names used by this program:
  typedef uint32_t name_size_type;
  std::unique_ptr< char[] > names;

  // Types that can index attributes, uniform variables, textures:
  typedef boost::uint_value_t< RSXGL_MAX_VERTEX_ATTRIBS - 1 >::least attrib_size_type;
  typedef boost::uint_value_t< RSXGL_MAX_PROGRAM_UNIFORM_COMPONENTS - 1 >::least uniform_size_type;
  typedef boost::uint_value_t< RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS - 1 >::least texture_size_type;

  // Table:
  template< typename Value >
  struct table_t {
    typedef std::vector< std::pair< name_size_type, Value > > type;

    struct lt {
      const char * names;

      lt(const char * _names) : names(_names) {
      }

      bool operator()(const typename type::value_type & lhs,const char * rhs) const {
	return strcmp(names + lhs.first,rhs) < 0;
      }

      bool operator()(const char * lhs,const typename type::value_type & rhs) const {
	return strcmp(lhs,names + rhs.first) < 0;
      }
    };
    
    static std::pair< typename type::iterator, bool > find(const char * names,type & t,const char * key) {
      auto tmp = std::equal_range(t.begin(),t.end(),key,lt(names));
      return std::make_pair(tmp.first,tmp.first != t.end());
    }

    static std::pair< typename type::const_iterator, bool > find(const char * names,const type & t,const char * key) {
      auto tmp = std::equal_range(t.begin(),t.end(),key,lt(names));
      return std::make_pair(tmp.first,tmp.first != t.end());
    }
  };

  // Tables of attributes, uniform variables, and texture maps:
  struct attrib_t {
    uint8_t type;
    attrib_size_type index, location;
  };

  struct uniform_t {
    uint8_t type;
    bit_set< RSXGL_MAX_SHADER_TYPES > invalid, enabled;
    uniform_size_type values_index, count, vp_index, program_offsets_index;
  };

  struct sampler_uniform_t {
    uint8_t type;
    boost::uint_value_t< RSXGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS >::least vp_index;
    boost::uint_value_t< RSXGL_MAX_TEXTURE_IMAGE_UNITS >::least fp_index;
  };

  table_t< attrib_t >::type attribs;
  table_t< uniform_t >::type uniforms;
  table_t< sampler_uniform_t >::type sampler_uniforms;

  name_size_type attrib_name_max_length, uniform_name_max_length;

  gl_shader_program * mesa_program;
  nvfx_vertex_program * nvfx_vp, * nvfx_streamvp;
  nvfx_fragment_program * nvfx_fp, * nvfx_streamfp;

  // --- hot:
  //
  // Type that can store instruction indices:
  typedef boost::uint_value_t< RSXGL_MAX_PROGRAM_INSTRUCTIONS - 1 >::least instruction_size_type;

  // Offset into microcode memory arenas. This is multiplied by the size of a single instruction
  // to return the effective address of a program's microcode:
  typedef uint32_t ucode_offset_type;

  ucode_offset_type vp_ucode_offset, fp_ucode_offset, streamvp_ucode_offset, streamfp_ucode_offset;
  instruction_size_type vp_num_insn, fp_num_insn, streamvp_num_insn, streamfp_num_insn;

  uint32_t vp_input_mask, vp_output_mask, vp_num_internal_const;
  uint32_t fp_control;
  uint32_t streamvp_input_mask, streamvp_output_mask, streamvp_num_internal_const;
  uint32_t streamfp_control, streamfp_num_outputs;
  uint32_t streamvp_vertexid_index, instanceid_index, point_sprite_control;
  bit_set< RSXGL_MAX_TEXTURE_COORDS > fp_texcoords, fp_texcoord2D, fp_texcoord3D;

  // Vertex attribs that are enabled:
  typedef smint_array< RSXGL_MAX_VERTEX_ATTRIBS, RSXGL_MAX_VERTEX_ATTRIBS > attrib_assignments_type;
  typedef bit_set< RSXGL_MAX_VERTEX_ATTRIBS > attribs_bitfield_type;

  attribs_bitfield_type attribs_enabled;
  attrib_assignments_type attrib_assignments;

  // Textures that are enabled:
  typedef smint_array< RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS > texture_assignments_type;
  typedef bit_set< RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS > textures_bitfield_type;

  textures_bitfield_type textures_enabled;
  texture_assignments_type texture_assignments;

  // Storage for uniform variable values:
  std::unique_ptr< ieee32_t[] > uniform_values;

  // Storage for uniform and texture program offsets:
  std::unique_ptr< instruction_size_type[] > program_offsets;
};

struct rsxgl_context_t;

void rsxgl_program_validate(rsxgl_context_t *,const uint32_t);
void rsxgl_feedback_program_validate(rsxgl_context_t *,const uint32_t);

#endif
