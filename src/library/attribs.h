//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// attribs.h - Private constants, types and functions related to vertex attributes.

#ifndef rsxgl_attribs_H
#define rsxgl_attribs_H

#include "gl_constants.h"
#include "rsxgl_limits.h"
#include "gl_object.h"
#include "arena.h"
#include "buffer.h"

#include "bit_set.h"
#include "smint_array.h"

#include "ieee32_t.h"

enum rsxgl_vertex_array_target {
  RSXGL_ACTIVE_VERTEX_ARRAY = 0,
  RSXGL_MAX_VERTEX_ARRAY_TARGETS = 1
};

enum rsxgl_attrib_types {
  RSXGL_VERTEX_S16_NR = 1,
  RSXGL_VERTEX_F32 = 2,
  RSXGL_VERTEX_F16 = 3,
  RSXGL_VERTEX_U8_NR = 4,
  RSXGL_VERTEX_S16_UN = 5,
  RSXGL_VERTEX_S11_11_10_NR = 6,
  RSXGL_VERTEX_U8_UN = 7
};

struct attribs_t {
  typedef bindable_gl_object< attribs_t, RSXGL_MAX_VERTEX_ARRAYS, RSXGL_MAX_VERTEX_ARRAY_TARGETS, 1 > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;
  object_container_type< buffer_t, RSXGL_MAX_VERTEX_ATTRIBS > buffers;

  ieee32_t defaults[RSXGL_MAX_VERTEX_ATTRIBS][4];

  bit_set< RSXGL_MAX_VERTEX_ATTRIBS > enabled;
  uint32_t offset[RSXGL_MAX_VERTEX_ATTRIBS];
  smint_array< 15, RSXGL_MAX_VERTEX_ATTRIBS > type;
  smint_array< 3, RSXGL_MAX_VERTEX_ATTRIBS > size;
  uint8_t stride[RSXGL_MAX_VERTEX_ATTRIBS];

#if (RSXGL_CONFIG_client_attribs == 1)
  bit_set< RSXGL_MAX_VERTEX_ATTRIBS > client_buffer_enabled;
  void * client_buffer[RSXGL_MAX_VERTEX_ATTRIBS];
#endif

  attribs_t() {
    for(size_t i = 0;i < RSXGL_MAX_VERTEX_ATTRIBS;++i) {
      defaults[i][0].f = 0.0f;
      defaults[i][1].f = 0.0f;
      defaults[i][2].f = 0.0f;
      defaults[i][3].f = 1.0f;
      offset[i] = 0;

#if (RSXGL_CONFIG_client_attribs == 1)
      client_buffer[i] = 0;
#endif
    }
  }

  ~attribs_t();

  void destroy();
};

struct rsxgl_context_t;

void rsxgl_attribs_validate(rsxgl_context_t *,const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > &,const uint32_t,const uint32_t,const uint32_t);

#endif
