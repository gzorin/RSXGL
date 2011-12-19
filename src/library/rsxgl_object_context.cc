#include "rsxgl_object_context.h"

extern "C" mspace rsxgl_rsx_mspace();

static void
rsxgl_init_default_arena(void * ptr)
{
  memory_arena_t::storage_type * storage = (memory_arena_t::storage_type *)ptr;

  gcmConfiguration config;
  gcmGetConfiguration(&config);
  uint32_t offset;
  gcmAddressToOffset(config.localAddress,&offset);
  
  memory_arena_t & arena = storage -> at(0);
  
  arena.address = config.localAddress;
  arena.space = rsxgl_rsx_mspace();
  arena.memory.location = RSXGL_MEMORY_LOCATION_LOCAL;
  arena.memory.offset = offset;
  arena.size = config.localSize;
}

static void
rsxgl_init_default_framebuffer(void * ptr)
{
  framebuffer_t::storage_type * storage = (framebuffer_t::storage_type *)ptr;
  framebuffer_t & framebuffer = storage -> at(0);
  framebuffer.is_default = 1;
}

rsxgl_object_context_t::rsxgl_object_context_t()
  : m_refCount(0), m_arena_storage(0,rsxgl_init_default_arena), m_attribs_storage(0,0), m_sampler_storage(0,0), m_texture_storage(0,0), m_framebuffer_storage(0,rsxgl_init_default_framebuffer)
{
}
