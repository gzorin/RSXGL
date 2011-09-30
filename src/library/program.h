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
#include "array.h"
#include "smint_array.h"

#include <nv40prog.h>

#include <cstddef>
#include <cassert>

#include <boost/integer.hpp>

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

  typedef array< char, uint32_t > source_type;
  typedef array< uint8_t, uint32_t > binary_type;
  typedef array< char, uint32_t > info_type;

  source_type::size_type source_size;
  binary_type::size_type binary_size;
  info_type::size_type info_size;

  source_type::pointer_type source_data;
  binary_type::pointer_type binary_data;
  info_type::pointer_type info_data;

  source_type::type source() { return source_type::type(source_data,source_size); }
  source_type::const_type source() const { return source_type::const_type(source_data,source_size); }

  binary_type::type binary() { return binary_type::type(binary_data,binary_size); }
  binary_type::const_type binary() const { return binary_type::const_type(binary_data,binary_size); }

  info_type::type info() { return info_type::type(info_data,info_size); }
  info_type::const_type info() const { return info_type::const_type(info_data,info_size); }
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

  typedef object_container_type< shader_t, RSXGL_MAX_SHADER_TYPES > shader_container_type;

  shader_container_type attached_shaders, linked_shaders;

  // Information returned from glLinkProgram():
  typedef array< char, uint32_t > info_type;
  info_type::size_type info_size;
  info_type::pointer_type info_data;

  info_type::type info() { return info_type::type(info_data,info_size); }
  info_type::const_type info() const { return info_type::const_type(info_data,info_size); }

  // Accumulate all of the names used by this program:
  typedef uint32_t name_size_type;
  typedef array< char, name_size_type > names_type;

  names_type::size_type names_size;
  names_type::pointer_type names_data;

  names_type::type names() { return names_type::type(names_data,names_size); }
  names_type::const_type names() const { return names_type::const_type(names_data,names_size); }

  // Types that can index attributes, uniform variables, textures:
  typedef boost::uint_value_t< RSXGL_MAX_VERTEX_ATTRIBS - 1 >::least attrib_size_type;
  typedef boost::uint_value_t< RSXGL_MAX_PROGRAM_UNIFORM_COMPONENTS - 1 >::least uniform_size_type;
  typedef boost::uint_value_t< RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS - 1 >::least texture_size_type;

  // Table:
  template< typename Value, typename SizeType >
  struct table {
    typedef std::pair< name_size_type, Value > value_type;
    typedef array< value_type, SizeType > array_type;

    typedef typename array_type::size_type size_type;
    typedef typename array_type::pointer_type pointer_type;

    struct find_lt {
      names_type::const_type & names;

      find_lt(names_type::const_type & _names) : names(_names) {}

      bool operator()(const value_type & lhs,const char * rhs) const {
	return strcmp(names.values + lhs.first,rhs) < 0;
      }
    };

    struct find_eq {
      names_type::const_type & names;

      find_eq(names_type::const_type & _names) : names(_names) {}

      bool operator()(const value_type & lhs,const char * rhs) const {
	return strcmp(names.values + lhs.first,rhs) == 0;
      }
    };

    struct type : public array_type::type {
      typedef typename array_type::type base_type;

      type(pointer_type & _values,size_type & _size)
	: base_type(_values,_size) {
      }

      std::pair< bool, size_type > find(names_type::const_type names,const char * name) const {
	pointer_type it = std::lower_bound(base_type::values,base_type::values + base_type::size,name,find_lt(names));
	if(it != (base_type::values + base_type::size) && find_eq(names)(*it,name)) {
	  return std::make_pair(true,it - base_type::values);
	}
	else {
	  return std::make_pair(false,0);
	}
      }
    };

    struct const_type : public array_type::const_type {
      typedef typename array_type::const_type base_type;

      const_type(const pointer_type & _values,const size_type & _size)
	: base_type(_values,_size) {
      }

      std::pair< bool, size_type > find(names_type::const_type names,const char * name) const {
	pointer_type it = std::lower_bound(base_type::values,base_type::values + base_type::size,name,find_lt(names));
	if(it != (base_type::values + base_type::size) && find_eq(names)(*it,name)) {
	  return std::make_pair(true,it - base_type::values);
	}
	else {
	  return std::make_pair(false,0);
	}
      }
    };
  };

  // Tables of attributes, uniform variables, and texture maps:
  struct attrib_t {
    uint8_t type;
    attrib_size_type index;
  };

  struct uniform_t {
    uint8_t type;
    bit_set< RSXGL_MAX_SHADER_TYPES > invalid, enabled;
    uniform_size_type values_index, count, vp_index, program_offsets_index;
  };

  typedef table< attrib_t, attrib_size_type > attrib_table_type;
  typedef table< uniform_t, uniform_size_type > uniform_table_type;

  attrib_table_type::size_type attrib_table_size;
  uniform_table_type::size_type uniform_table_size;
  
  attrib_table_type::pointer_type attrib_table_values;
  uniform_table_type::pointer_type uniform_table_values;

  name_size_type attrib_name_max_length, uniform_name_max_length;

  attrib_table_type::type attrib_table() { return attrib_table_type::type(attrib_table_values,attrib_table_size); }
  attrib_table_type::const_type attrib_table() const { return attrib_table_type::const_type(attrib_table_values,attrib_table_size); }

  uniform_table_type::type uniform_table() { return uniform_table_type::type(uniform_table_values,uniform_table_size); }
  uniform_table_type::const_type uniform_table() const { return uniform_table_type::const_type(uniform_table_values,uniform_table_size); }

  // Attribute bindings requested by glBindAttribLocation(). A string for each possible vertex attribute:
  typedef array< char, uint32_t > attrib_binding_type;
  attrib_binding_type::size_type attrib_binding_size[RSXGL_MAX_VERTEX_ATTRIBS];
  attrib_binding_type::pointer_type attrib_binding_data[RSXGL_MAX_VERTEX_ATTRIBS];

  attrib_binding_type::type attrib_binding(size_t i) { assert(i < RSXGL_MAX_VERTEX_ATTRIBS); return attrib_binding_type::type(attrib_binding_data[i],attrib_binding_size[i]); }
  attrib_binding_type::const_type attrib_binding(size_t i) const { assert(i < RSXGL_MAX_VERTEX_ATTRIBS); return attrib_binding_type::const_type(attrib_binding_data[i],attrib_binding_size[i]); }

  // --- hot:
  //
  // Type used to manipulate program instructions:
  union vp_instruction_type {
    uint32_t dwords[4];
  };

  union fp_instruction_type {
    uint32_t dwords[4];
  };

  // Type that can store instruction indices:
  typedef boost::uint_value_t< RSXGL_MAX_PROGRAM_INSTRUCTIONS - 1 >::least instruction_size_type;

  // Offset into microcode memory arenas. This is multiplied by the size of a single instruction
  // to return the effective address of a program's microcode:
  typedef uint32_t ucode_offset_type;

  ucode_offset_type vp_ucode_offset, fp_ucode_offset;
  instruction_size_type vp_num_insn, fp_num_insn;

  uint32_t vp_input_mask, vp_output_mask, vp_num_internal_const;
  uint32_t fp_control, fp_num_regs;

  // Vertex attribs that are enabled:
  bit_set< RSXGL_MAX_VERTEX_ATTRIBS > attribs_enabled;

  // Textures that are enabled:
  bit_set< RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS > textures_enabled;

  // Storage for uniform variable values:
  ieee32_t * uniform_values;

  // Storage for uniform and texture program offsets:
  instruction_size_type * program_offsets;
};

struct rsxgl_context_t;

void rsxgl_program_validate(rsxgl_context_t *,const uint32_t);

#endif
