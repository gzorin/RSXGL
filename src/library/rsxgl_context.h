//-*-C-*-

#ifndef rsxgl_context_H
#define rsxgl_context_H

#include "rsxgl_assert.h"
#include "gcm.h"
#include "egl_types.h"
#include "arena.h"
#include "buffer.h"
#include "state.h"
#include "attribs.h"
#include "uniforms.h"
#include "textures.h"
#include "program.h"
#include "framebuffer.h"

#include "bit_set.h"

#include <stddef.h>

struct rsxgl_context_t {
  struct rsxegl_context_t base;
  struct state_t state;

  memory_arena_t::binding_type arena_binding;
  buffer_t::binding_type buffer_binding;

  bit_set< RSXGL_MAX_VERTEX_ATTRIBS > invalid_attribs;
  attribs_t::binding_type attribs_binding;

  texture_t::binding_type::size_type active_texture;
  texture_t::binding_bitfield_type invalid_textures;
  sampler_t::binding_bitfield_type invalid_samplers;
  texture_t::binding_type texture_binding;
  sampler_t::binding_type sampler_binding;

  renderbuffer_t::binding_type renderbuffer_binding;
  framebuffer_t::binding_type framebuffer_binding;
  
  program_t::binding_type program_binding;

  // Used by glFinish():
  uint32_t ref;

  uint8_t timestamp_sync;

  // Next timestamp to be given out when draw functions are initiated.
  // Should be initialized to 1:
  uint32_t next_timestamp, last_timestamp;

  // Cached copy of the current timestamp on the GPU:
  // Should be initialized to 0:
  uint32_t cached_timestamp;

  rsxgl_context_t()
  : active_texture(0), ref(0), timestamp_sync(0), next_timestamp(1), last_timestamp(0), cached_timestamp(0) {
  }

  inline
  gcmContextData * gcm_context() {
    rsxgl_assert(base.gcm_context != 0);
    return base.gcm_context;
  }

  static void egl_callback(rsxegl_context_t *,const uint8_t);
  static void timestamp_overflow(void *);
};

extern rsxgl_context_t * rsxgl_ctx;

static inline rsxgl_context_t *
current_ctx()
{
  assert(rsxgl_ctx != 0);
  return rsxgl_ctx;
}

uint32_t rsxgl_timestamp_create(rsxgl_context_t *);
void rsxgl_timestamp_wait(rsxgl_context_t *,const uint32_t);
void rsxgl_timestamp_post(rsxgl_context_t *,const uint32_t);

#endif
