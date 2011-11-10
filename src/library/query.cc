//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// query.cc - OpenGL query objects.

#include "rsxgl_context.h"
#include "query.h"

#include <GL3/gl3.h>
#include "error.h"

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

// Manage the hardware-limited namespace of query objects (2048 for the RSX). Re-use the capabilities
// of gl_object<> to do so.
struct rsxgl_query_object_t {
  typedef gl_object< rsxgl_query_object_t, RSXGL_MAX_QUERY_OBJECTS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;

  static storage_type & storage();

  rsxgl_query_object_t() {}
  ~rsxgl_query_object_t() {}
};

rsxgl_query_object_t::storage_type &
rsxgl_query_object_t::storage()
{
  static rsxgl_query_object_t::storage_type _storage(RSXGL_MAX_QUERY_OBJECTS);
  return _storage;
}

static inline rsxgl_query_object_t::name_type
rsxgl_query_object_really_allocate()
{
  const rsxgl_query_object_t::name_type name = rsxgl_query_object_t::storage().create_name();
  if(name > RSXGL_MAX_QUERY_OBJECTS) {
    rsxgl_query_object_t::storage().destroy(name);
    return 0;
  }
  else {
    rsxgl_assert(name > 0);
    rsxgl_query_object_t::storage().create_object(name);
    return name;
  }
}

static inline void
rsxgl_query_object_really_free(const rsxgl_query_object_t::name_type name)
{
  if(rsxgl_query_object_t::storage().is_name(name)) {
    rsxgl_assert(rsxgl_query_object_t::storage().is_object(name));
    rsxgl_query_object_t::storage().destroy(name);
  }
}

#if 0
static inline query_t::index_type
rsxgl_sync_name_to_rsx_index(const rsxgl_query_object_t::name_type name)
{
  rsxgl_assert(name > 0 && name <= RSXGL_MAX_QUERY_OBJECTS);
  return (name - 1);
}

static inline rsxgl_query_object_t::name_type
rsxgl_rsx_index_to_sync_name(const query_t::index_type index)
{
  rsxgl_assert(index < RSXGL_MAX_QUERY_OBJECTS);
  return (index) + 1;
}

query_t::index_type
rsxgl_query_object_allocate()
{
  rsxgl_query_object_t::name_type name = rsxgl_query_object_really_allocate();
  return (name == 0) ? 0 : rsxgl_sync_name_to_rsx_index(name);
}

void
rsxgl_query_object_free(const query_t::index_type index)
{
  rsxgl_query_object_really_free(rsxgl_rsx_index_to_sync_name(index));
}
#endif

//
query_t::storage_type & query_t::storage()
{
  static query_t::storage_type _storage;
  return _storage;
}

query_t::~query_t()
{
  if(type != RSXGL_MAX_QUERY_TARGETS) {
    
  }
}

GLAPI void APIENTRY
glGenQueries (GLsizei n, GLuint *ids)
{
  GLsizei count = query_t::storage().create_names(n,ids);

  if(count != n) {
    RSXGL_ERROR_(GL_OUT_OF_MEMORY);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDeleteQueries (GLsizei n, const GLuint *ids)
{
  struct rsxgl_context_t * ctx = current_ctx();

  for(GLsizei i = 0;i < n;++i,++ids) {
    const GLuint query_name = *ids;

    if(query_name == 0) continue;

    // Free resources used by this object:
    if(query_t::storage().is_object(query_name)) {
      // Block for something?
      query_t & query = query_t::storage().at(query_name);
      if(query.type != RSXGL_MAX_QUERY_TARGETS) {
      }

      // Destroy it:
      query_t::storage().destroy(query_name);
    }
    // It was just a name:
    else if(query_t::storage().is_name(query_name)) {
      query_t::storage().destroy(query_name);
    }
  }

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsQuery (GLuint id)
{
  return query_t::storage().is_object(id);
}

static inline uint8_t
rsxgl_query_target(const GLenum target)
{
  switch(target) {
  case GL_SAMPLES_PASSED:
    return RSXGL_QUERY_SAMPLES_PASSED;
  case GL_ANY_SAMPLES_PASSED:
    return RSXGL_QUERY_ANY_SAMPLES_PASSED;
  case GL_PRIMITIVES_GENERATED:
    return RSXGL_QUERY_PRIMITIVES_GENERATED;
  case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
    return RSXGL_QUERY_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN;
  case GL_TIME_ELAPSED:
    return RSXGL_QUERY_TIME_ELAPSED;
  default:
    return RSXGL_MAX_QUERY_TARGETS;
  }
}

GLAPI void APIENTRY
glBeginQuery (GLenum target, GLuint id)
{
  const uint8_t rsx_target = rsxgl_query_target(target);
  if(rsx_target == RSXGL_MAX_QUERY_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(!ctx -> query_binding.is_anything_bound(rsx_target) != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(id == 0 || !query_t::storage().is_name(id)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  query_t & query = query_t::storage().at(id);

  if(query.type == RSXGL_MAX_QUERY_TARGETS) {
    query.type = rsx_target;
    // allocate the index:
  }
  else if(query.type != rsx_target) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }
  
  ctx -> query_binding.bind(rsx_target,id);

  if(query.type == RSXGL_QUERY_SAMPLES_PASSED || query.type == RSXGL_QUERY_ANY_SAMPLES_PASSED) {
    uint32_t * buffer = gcm_reserve(context,4);
    
    gcm_emit_method_at(buffer,0,NV30_3D_QUERY_ENABLE,1);
    gcm_emit_at(buffer,1,1);

    gcm_emit_method_at(buffer,2,NV30_3D_QUERY_ENABLE,1);
    gcm_emit_at(buffer,3,1);

    gcm_finish_n_commands(context,4);
  }
  else if(query.type == RSXGL_QUERY_TIME_ELAPSED) {
    
  }
  else {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glEndQuery (GLenum target)
{
  const uint8_t rsx_target = rsxgl_query_target(target);
  if(rsx_target == RSXGL_MAX_QUERY_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> query_binding.is_anything_bound(rsx_target) != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  query_t & query = query_t::storage().at(id);

  if(query.type == RSXGL_QUERY_SAMPLES_PASSED || query.type == RSXGL_QUERY_ANY_SAMPLES_PASSED) {
    uint32_t * buffer = gcm_reserve(context,2);
    gcm_emit_method_at(buffer,0,NV30_3D_QUERY_GET,1);
    gcm_emit_at(buffer,1,(1 << 24) | (query.index << 8));
    gcm_finish_n_commands(context,2);
  }
  else {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  ctx -> query_binding.bind(rsx_target,0);

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetQueryiv (GLenum target, GLenum pname, GLint *params)
{
  const uint8_t rsx_target = rsxgl_query_target(target);
  if(rsx_target == RSXGL_MAX_QUERY_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> query_binding.is_anything_bound(rsx_target) != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  // TODO - finish this

  RSXGL_NOERROR_();
}

static inline void
rsxgl_get_query_object(GLuint id, GLenum pname, uint32_t * params)
{
  if(!query_t::storage().is_object(id)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  // TODO - finish this
}

GLAPI void APIENTRY
glGetQueryObjectiv (GLuint id, GLenum pname, GLint *params)
{
  uint32_t param = 0;
  rsxgl_get_query_object(id,pname,&param);
  if(!RSXGL_IS_ERROR()) *params = param;
}

GLAPI void APIENTRY
glGetQueryObjectuiv (GLuint id, GLenum pname, GLuint *params)
{
  uint32_t param = 0;
  rsxgl_get_query_object(id,pname,&param);
  if(!RSXGL_IS_ERROR()) *params = param;
}
