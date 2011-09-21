// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// arena.cc - Create arenas from which memory may be allocated.

#include "gcm.h"
#include "rsxgl_context.h"
#include "arena.h"
#include "gl_object_storage.h"

#include <GL3/gl3.h>
#include "GL3/rsxgl3ext.h"
#include "error.h"

#include <stddef.h>
#include <malloc.h>

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

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

memory_arena_t::storage_type & memory_arena_t::storage()
{
  static memory_arena_t::storage_type _storage(0,rsxgl_init_default_arena);
  return _storage;
}

memory_t
rsxgl_arena_allocate(memory_arena_t & arena,rsx_size_t align,rsx_size_t size,rsx_ptr_t * address)
{
  rsx_ptr_t addr = mspace_memalign(arena.space,align,size);

  if(addr == 0) {
    return memory_t();
  }
  else {
    if(address != 0) *address = addr;
    return memory_t(arena.memory.location,arena.memory.offset + ((uint8_t *)addr - (uint8_t *)(arena.address)),1);
  }
}

void
rsxgl_arena_free(struct memory_arena_t & arena,const struct memory_t & memory)
{
  mspace_free(arena.space,rsxgl_arena_address(arena,memory));
}

static inline size_t
rsxgl_memory_location(GLenum location)
{
  if(location == GL_MAIN_MEMORY_ARENA_RSX) {
    return RSXGL_MEMORY_LOCATION_MAIN;
  }
  else if(location == GL_GPU_MEMORY_ARENA_RSX) {
    return RSXGL_MEMORY_LOCATION_LOCAL;
  }
  else {
    return ~0U;
  }
}

GLAPI GLuint APIENTRY
glCreateMemoryArenaRSX(GLenum location,GLsizei align,GLsizei size)
{
  const size_t rsx_location = rsxgl_memory_location(location);
  if(rsx_location == ~0U) RSXGL_ERROR(GL_INVALID_ENUM,0);

  if(location == GL_MAIN_MEMORY_ARENA_RSX && ((align % (1024 * 1024) != 0) || (size % (1024 * 1024) != 0))) {
    RSXGL_ERROR(GL_INVALID_VALUE,0);
  }

  uint32_t name = memory_arena_t::storage().create_name_and_object();
  memory_arena_t & arena = memory_arena_t::storage().at(name);
  uint32_t offset = 0;

  if(rsx_location == RSXGL_MEMORY_LOCATION_LOCAL) {
    arena.address = rsxgl_rsx_memalign(align,size);
    if(arena.address == 0) RSXGL_ERROR(GL_OUT_OF_MEMORY,0);

    gcmAddressToOffset(arena.address,&offset);
  }
  else if(rsx_location == RSXGL_MEMORY_LOCATION_MAIN) {
    arena.address = memalign(align,size);
    if(arena.address == 0) RSXGL_ERROR(GL_OUT_OF_MEMORY,0);

    gcmMapMainMemory(arena.address,size,&offset);
  }

  arena.memory.location = rsx_location;
  arena.memory.offset = offset;
  arena.size = size;
  arena.space = create_mspace_with_base(arena.address,arena.size,0);

  RSXGL_NOERROR(name);
}

void
memory_arena_t::destroy()
{
  destroy_mspace(space);

  if(memory.location == RSXGL_MEMORY_LOCATION_LOCAL) {
    rsxgl_rsx_free(address);
  }
  else if(memory.location == RSXGL_MEMORY_LOCATION_MAIN) {
    free(address);
  }
}

GLAPI void APIENTRY
glDeleteMemoryArenaRSX(GLuint name)
{
  if(!memory_arena_t::storage().is_object(name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  memory_arena_t::storage().destroy(name);

  RSXGL_NOERROR_();
}

static inline size_t
rsxgl_arena_target(GLenum target)
{
  if(target == GL_BUFFER_ARENA_RSX) {
    return RSXGL_BUFFER_ARENA;
  }
  else if(target == GL_TEXTURE_ARENA_RSX) {
    return RSXGL_TEXTURE_ARENA;
  }
  else if(target == GL_RENDERBUFFER_ARENA_RSX) {
    return RSXGL_RENDERBUFFER_ARENA;
  }
  else {
    return ~0U;
  }
}

GLAPI void APIENTRY
glUseMemoryArenaRSX(GLenum target,GLuint name)
{
  uint32_t rsx_target = rsxgl_arena_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!(name == 0 || memory_arena_t::storage().is_object(name))) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  struct rsxgl_context_t * ctx = current_ctx();
  ctx -> arena_binding.bind(rsx_target,name);
}

GLAPI void APIENTRY
glGetMemoryArenaParameterivRSX(GLenum target,GLenum pname,GLint * params)
{
  const size_t rsx_target = rsxgl_arena_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  struct rsxgl_context_t * ctx = current_ctx();

  memory_arena_t & arena = ctx -> arena_binding[rsx_target];

  if(pname == GL_ARENA_SIZE_RSX) {
    *params = arena.size;
  }
  else if(pname == GL_ARENA_LOCATION_RSX) {
    if(arena.memory.location == RSXGL_MEMORY_LOCATION_LOCAL) {
      *params = GL_GPU_MEMORY_ARENA_RSX;
    }
    else if(arena.memory.location == RSXGL_MEMORY_LOCATION_MAIN) {
      *params = GL_MAIN_MEMORY_ARENA_RSX;
    }
    else {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetMemoryArenaPointervRSX(GLenum target,GLenum pname,GLvoid ** params)
{
  const size_t rsx_target = rsxgl_arena_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  struct rsxgl_context_t * ctx = current_ctx();

  memory_arena_t & arena = ctx -> arena_binding[rsx_target];

  if(pname == GL_ARENA_POINTER_RSX) {
    *params = arena.address;
  }

  RSXGL_NOERROR_();
}
