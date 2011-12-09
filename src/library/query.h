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

#include <boost/integer.hpp>

// Manage RSX "reports":
typedef boost::uint_value_t< RSXGL_MAX_QUERY_OBJECTS >::least rsxgl_query_object_index_type;

// Return value of RSXGL_MAX_QUERY_OBJECTS is failure:
rsxgl_query_object_index_type rsxgl_query_object_allocate();
void rsxgl_query_object_free(rsxgl_query_object_index_type);

static inline void
rsxgl_query_object_reset(gcmContextData * context)
{
  uint32_t * buffer = gcm_reserve(context,2);

  gcm_emit_method_at(buffer,0,NV30_3D_QUERY_RESET,1);
  gcm_emit_at(buffer,1,1);

  gcm_finish_n_commands(context,2);
}

static inline void
rsxgl_query_object_enable_samples(gcmContextData * context,const bool samples)
{
  uint32_t * buffer = gcm_reserve(context,2);

  gcm_emit_method_at(buffer,0,NV30_3D_QUERY_ENABLE,1);
  gcm_emit_at(buffer,1,samples);

  gcm_finish_n_commands(context,2);
}

static inline void
rsxgl_query_object_set(gcmContextData * context,const rsxgl_query_object_index_type index)
{
  uint32_t * buffer = gcm_reserve(context,2);

  gcm_emit_method_at(buffer,0,NV30_3D_QUERY_GET,1);
  gcm_emit_at(buffer,1,(1 << 24) | (index << 4));

  gcm_finish_n_commands(context,2);
}

static inline uint32_t
rsxgl_query_object_get_value(const rsxgl_query_object_index_type index)
{
  volatile gcmReportData * report = gcmGetReportDataAddress(index);
  rsxgl_assert(report != 0);
  return report -> value;
}

static inline uint64_t
rsxgl_query_object_get_timestamp(const rsxgl_query_object_index_type index)
{
  volatile gcmReportData * report = gcmGetReportDataAddress(index);
  rsxgl_assert(report != 0);
  return report -> timer;
}

enum rsxgl_query_target {
  RSXGL_QUERY_SAMPLES_PASSED = 0,
  RSXGL_QUERY_ANY_SAMPLES_PASSED = 1,
  RSXGL_QUERY_PRIMITIVES_GENERATED = 2,
  RSXGL_QUERY_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = 3,
  RSXGL_QUERY_TIME_ELAPSED = 4,
  RSXGL_QUERY_TIMESTAMP = 5,
  RSXGL_MAX_QUERY_TARGETS = 6
};

enum rsxgl_query_status {
  RSXGL_QUERY_STATUS_INACTIVE = 0,
  RSXGL_QUERY_STATUS_ACTIVE = 1,
  RSXGL_QUERY_STATUS_PENDING = 2,
  RSXGL_QUERY_STATUS_CACHED = 3
};

struct query_t {
  typedef bindable_gl_object< query_t, RSXGL_MAX_QUERIES, RSXGL_MAX_QUERY_TARGETS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  /// \brief Type of query object:
  uint8_t type:6, status:2;

  /// \brief RSX report indices - two are needed for measuring elapsed time:
  rsxgl_query_object_index_type index;

  //
  /// \brief Timestamp - point in the command stream when the query will be finished:
  uint32_t timestamps[2];

  /// \brief Cached value:
  uint64_t value;

  query_t()
    : type(RSXGL_MAX_QUERY_TARGETS), status(RSXGL_QUERY_STATUS_INACTIVE), index(RSXGL_MAX_QUERY_OBJECTS), value(0) {
    timestamps[0] = 0;
    timestamps[1] = 0;
  }

  ~query_t();
};

#endif
