//-*-C++-*-

#ifndef rsxgl_object_context_H
#define rsxgl_object_context_H

#include <stdint.h>
#include "arena.h"
#include "buffer.h"
#include "attribs.h"
#include "textures.h"
#include "program.h"
#include "framebuffer.h"
#include "query.h"

struct rsxgl_object_context_t {

  rsxgl_object_context_t();

  inline
  memory_arena_t::storage_type & arena_storage() {
    return m_arena_storage;
  }

  inline
  buffer_t::storage_type & buffer_storage() {
    return m_buffer_storage;
  }

  inline
  attribs_t::storage_type & attribs_storage() {
    return m_attribs_storage;
  }

  inline
  sampler_t::storage_type & sampler_storage() {
    return m_sampler_storage;
  }

  inline
  texture_t::storage_type & texture_storage() {
    return m_texture_storage;
  }

  inline
  shader_t::storage_type & shader_storage() {
    return m_shader_storage;
  }

  inline
  program_t::storage_type & program_storage() {
    return m_program_storage;
  }

  inline
  renderbuffer_t::storage_type & renderbuffer_storage() {
    return m_renderbuffer_storage;
  }

  inline
  framebuffer_t::storage_type & framebuffer_storage() {
    return m_framebuffer_storage;
  }

  inline
  query_t::storage_type & query_storage() {
    return m_query_storage;
  }

private:

  uint32_t m_refCount;

  memory_arena_t::storage_type m_arena_storage;
  buffer_t::storage_type m_buffer_storage;
  attribs_t::storage_type m_attribs_storage;
  sampler_t::storage_type m_sampler_storage;
  texture_t::storage_type m_texture_storage;
  shader_t::storage_type m_shader_storage;
  program_t::storage_type m_program_storage;
  renderbuffer_t::storage_type m_renderbuffer_storage;
  framebuffer_t::storage_type m_framebuffer_storage;
  query_t::storage_type m_query_storage;
};

#endif
