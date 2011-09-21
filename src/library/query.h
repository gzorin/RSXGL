//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// query.h - OpenGL query objects.

#ifndef rsxgl_query_H
#define rsxgl_query_H

#include "gl_constants.h"
#include "rsxgl_limits.h"
#include "gl_object.h"

enum rsxgl_query_target {
  RSXGL_QUERY_SAMPLES_PASSED = 0,
  RSXGL_QUERY_ANY_SAMPLES_PASSED = 1,
  RSXGL_QUERY_PRIMITIVES_GENERATED = 2,
  RSXGL_QUERY_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = 3,
  RSXGL_QUERY_TIME_ELAPSED = 4,
  RSXGL_MAX_QUERY_TARGETS = 5
};

struct query_t {
  typedef bindable_gl_object< query_t, RSXGL_MAX_QUERIES, RSXGL_MAX_QUERY_TARGETS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  //
  /// \brief Timestamp - point in the command stream when the query will be finished:
  uint32_t timestamp;

  /// \brief Type of query object:
  uint8_t type;

  /// \brief Synchronization object index obtained from rsxgl_sync_object_allocate():
  uint8_t index;

  query_t()
    : timestamp(0), type(~0), index(0) {
  }

  ~query_t();
};

#endif
