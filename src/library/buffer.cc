// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// buffer.cc - Manage buffer objects.

#include "rsxgl_context.h"
#include <rsx/gcm_sys.h>
#include "gl_fifo.h"
#include "buffer.h"
#include "timestamp.h"
#include "attribs.h"

#include <GL3/gl3.h>
#include "error.h"

#include <stddef.h>
#include <string.h>

#include <unistd.h>

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

buffer_t::storage_type & buffer_t::storage()
{
  return current_object_ctx() -> buffer_storage();
}

buffer_t::~buffer_t()
{
  // Free memory used by this buffer:
  if(memory.offset != 0) {
    rsxgl_arena_free(memory_arena_t::storage().at(arena),memory);
  }
}

GLAPI void APIENTRY
glGenBuffers (GLsizei n, GLuint* buffers)
{
  GLsizei count = buffer_t::storage().create_names(n,buffers);

  if(count != n) {
    RSXGL_ERROR_(GL_OUT_OF_MEMORY);
  }

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsBuffer (GLuint buffer)
{
  return buffer_t::storage().is_object(buffer);
}

GLAPI void APIENTRY
glDeleteBuffers (GLsizei n, const GLuint* buffers)
{
  struct rsxgl_context_t * ctx = current_ctx();

  for(GLsizei i = 0;i < n;++i,++buffers) {
    const GLuint buffer_name = *buffers;

    if(buffer_name == 0) continue;

    // Free resources used by this object:
    if(buffer_t::storage().is_object(buffer_name)) {
      ctx -> buffer_binding.unbind_from_all(buffer_name);

      // TODO - orphan it, instead of doing this:
      buffer_t & buffer = buffer_t::storage().at(buffer_name);
      if(buffer.timestamp > 0) {
	rsxgl_timestamp_wait(ctx,buffer.timestamp);
	buffer.timestamp = 0;
      }

      // 
      buffer_t::gl_object_type::maybe_delete(buffer_name);
    }
    // It was just a name:
    else if(buffer_t::storage().is_name(buffer_name)) {
      buffer_t::storage().destroy(buffer_name);
    }
  }

  RSXGL_NOERROR_();
}

static inline size_t
rsxgl_buffer_target(GLenum target)
{
  switch(target) {
  case GL_ARRAY_BUFFER:
    return RSXGL_ARRAY_BUFFER;
  case GL_COPY_READ_BUFFER:
    return RSXGL_COPY_READ_BUFFER;
  case GL_COPY_WRITE_BUFFER:
    return RSXGL_COPY_WRITE_BUFFER;
  case GL_ELEMENT_ARRAY_BUFFER:
    return RSXGL_ELEMENT_ARRAY_BUFFER;
  case GL_PIXEL_PACK_BUFFER:
    return RSXGL_PIXEL_PACK_BUFFER;
  case GL_PIXEL_UNPACK_BUFFER:
    return RSXGL_PIXEL_UNPACK_BUFFER;
  case GL_TEXTURE_BUFFER:
    return RSXGL_TEXTURE_BUFFER;
  case GL_TRANSFORM_FEEDBACK_BUFFER:
    return RSXGL_TRANSFORM_FEEDBACK_BUFFER;
  case GL_UNIFORM_BUFFER:
    return RSXGL_UNIFORM_BUFFER;
  default:
    return ~0U;
  };
}

static inline int
rsxgl_buffer_usage(GLenum usage)
{
  switch(usage) {
  case GL_STREAM_DRAW:
    return RSXGL_STREAM_DRAW;
  case GL_STREAM_READ:
    return RSXGL_STREAM_READ;
  case GL_STREAM_COPY:
    return RSXGL_STREAM_COPY;
  case GL_STATIC_DRAW:
    return RSXGL_STATIC_DRAW;
  case GL_STATIC_READ:
    return RSXGL_STATIC_READ;
  case GL_STATIC_COPY:
    return RSXGL_STATIC_COPY;
  case GL_DYNAMIC_DRAW:
    return RSXGL_DYNAMIC_DRAW;
  case GL_DYNAMIC_READ:
    return RSXGL_DYNAMIC_READ;
  case GL_DYNAMIC_COPY:
    return RSXGL_DYNAMIC_COPY;
  default:
    return ~0;
  }
}

static inline int
rsxgl_buffer_access(GLenum access)
{
  switch(access) {
  case GL_READ_ONLY:
    return RSXGL_READ_ONLY;
  case GL_WRITE_ONLY:
    return RSXGL_WRITE_ONLY;
  case GL_READ_WRITE:
    return RSXGL_READ_WRITE;
  default:
    return ~0;
  }
}

static inline int
rsxgl_buffer_valid_range(const struct buffer_t & buffer,uint32_t offset,uint32_t length)
{
  if((offset + length) > buffer.size) {
    return 0;
  }
  else {
    return 1;
  }
}

GLAPI void APIENTRY
glBindBuffer (GLenum target, GLuint buffer_name)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!(buffer_name == 0 || buffer_t::storage().is_name(buffer_name))) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  struct rsxgl_context_t * ctx = current_ctx();

  // Create the buffer_name object if it doesn't exist yet (buffer_name in memory doesn't get allocated until glBufferData()):
  if(buffer_name != 0 && !buffer_t::storage().is_object(buffer_name)) {
    buffer_t::storage().create_object(buffer_name);
  }

  ctx -> buffer_binding.bind(rsx_target,buffer_name);
  
  RSXGL_NOERROR_();
}

static inline void
rsxgl_bind_buffer_range(GLenum target, GLuint index, GLuint buffer_name, rsx_size_t offset, rsx_size_t size)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(!(rsx_target == RSXGL_TRANSFORM_FEEDBACK_BUFFER || rsx_target == RSXGL_UNIFORM_BUFFER)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if((rsx_target == RSXGL_TRANSFORM_FEEDBACK_BUFFER && index < RSXGL_MAX_TRANSFORM_FEEDBACK_BUFFER_BINDINGS) ||
     (rsx_target == RSXGL_UNIFORM_BUFFER && index < RSXGL_MAX_UNIFORM_BUFFER_BINDINGS)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(!(buffer_name == 0 || buffer_t::storage().is_object(buffer_name))) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(buffer_name != 0 && buffer_t::storage().at(buffer_name).size == 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  struct rsxgl_context_t * ctx = current_ctx();

  if(rsx_target == RSXGL_TRANSFORM_FEEDBACK_BUFFER && ctx -> state.enable.transform_feedback_mode != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  ctx -> buffer_binding.bind(rsx_target,buffer_name);

  const size_t i =
    (rsx_target == RSXGL_TRANSFORM_FEEDBACK_BUFFER) ? (RSXGL_TRANSFORM_FEEDBACK_BUFFER0 + index) :
    (rsx_target == RSXGL_UNIFORM_BUFFER) ? (RSXGL_UNIFORM_BUFFER0 + index) :
    ~0;

  ctx -> buffer_binding.bind(i,buffer_name);
  ctx -> buffer_binding_offset_size[i] = std::make_pair((rsx_size_t)offset,(rsx_size_t)size);

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glBindBufferRange (GLenum target, GLuint index, GLuint buffer_name, GLintptr offset, GLsizeiptr size)
{
  rsxgl_bind_buffer_range(target,index,buffer_name,offset,(rsx_size_t)size);
}

GLAPI void APIENTRY
glBindBufferBase (GLenum target, GLuint index, GLuint buffer_name)
{
  rsxgl_bind_buffer_range(target,index,buffer_name,0,(rsx_size_t)~0);
}

GLAPI void APIENTRY
glBufferData (GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
  if(size < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(ctx -> buffer_binding[rsx_target].mapped != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  int rsx_usage = rsxgl_buffer_usage(usage);
  if(rsx_usage < 0) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  buffer_t * buffer = &ctx -> buffer_binding[rsx_target];

#if 0
  // If a pending GPU operation uses this buffer, then orphan it:
  if((buffer -> timestamp != 0) && (!rsxgl_timestamp_passed(ctx -> cached_timestamp,ctx -> timestamp_sync,buffer -> timestamp))) {
    buffer_t::storage().orphan(ctx -> buffer_binding.names[rsx_target]);

    buffer -> timestamp = 0;
    buffer -> invalid = 0;
    buffer -> usage = 0;
    buffer -> mapped = 0;
    buffer -> memory = memory_t();
    buffer -> arena = 0;
    buffer -> size = 0;
    buffer -> mapped_offset = 0;
    buffer -> mapped_size = 0;
  }
#else
  if(buffer -> timestamp > 0) {
    rsxgl_timestamp_wait(ctx,buffer -> timestamp);
    buffer -> timestamp = 0;
  }

  // Free the old buffer:
  if(buffer -> memory.offset != 0) {
    rsxgl_arena_free(memory_arena_t::storage().at(buffer -> arena),buffer -> memory);
    buffer -> memory = memory_t();
  }
#endif

  // If a buffer is actually being requested, then allocate memory for it:
  void * address = 0;
  
  if(size > 0) {
    buffer -> invalid = 1;
    buffer -> usage = rsx_usage;
    buffer -> arena = ctx -> arena_binding.names[RSXGL_BUFFER_ARENA];
    buffer -> memory = rsxgl_arena_allocate(memory_arena_t::storage().at(buffer -> arena),128,size,&address);
    
    if(!buffer -> memory) RSXGL_ERROR_(GL_OUT_OF_MEMORY);
    
    buffer -> size = size;
  }

  if(address != 0 && data != 0 && buffer -> size > 0) {
    memcpy(address,data,buffer -> size);
  }

  const buffer_t::name_type name = ctx -> buffer_binding.names[rsx_target];

  // See if the buffer is attached to the current vertex array object; if so, invalidate:
  attribs_t & attribs = ctx -> attribs_binding[0];
  for(size_t i = 0;i < RSXGL_MAX_VERTEX_ATTRIBS;++i) {
    if(attribs.buffers.is_bound(i,name)) {
      ctx -> invalid_attribs.set(i);
    }
  }

  // 
  for(size_t i = 0;i < RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;++i) {
    texture_t & texture = ctx -> texture_binding[i];
#if 0
    if(texture.buffer == name) {
    }
#endif
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }
  buffer_t & buffer = ctx -> buffer_binding[rsx_target];
  
  if(buffer.mapped != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(offset < 0 || size < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  if(!rsxgl_buffer_valid_range(buffer,offset,size)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  void * address = rsxgl_arena_address(memory_arena_t::storage().at(buffer.arena),buffer.memory);

  if(address != 0 && data != 0 && size > 0) {
    // TODO - if a pending operation depends upon this buffer, wait for that operation to finish:
    // TODO - replace this with something smarter that doesn't conservatively decide to block on the GPU:
    if(buffer.timestamp > 0) {
      rsxgl_timestamp_wait(ctx,buffer.timestamp);
      buffer.timestamp = 0;
    }
    
    // Copy the data:
    memcpy((uint8_t *)address + offset,data,size);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, GLvoid *data)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }
  buffer_t & buffer = ctx -> buffer_binding[rsx_target];
  
  if(buffer.mapped != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(offset < 0 || size < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  if(!rsxgl_buffer_valid_range(buffer,offset,size)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  void * address = rsxgl_arena_address(memory_arena_t::storage().at(buffer.arena),buffer.memory);

  if(address != 0 && data != 0 && size > 0) {
    // TODO - wait for pending operations to finish:

    // Copy it:
    memcpy(data,(uint8_t *)address + offset,size);
  }

  RSXGL_NOERROR_();
}

static inline void *
rsxgl_map_buffer_range(rsxgl_context_t * ctx,const buffer_t::name_type buffer_name,const uint32_t offset, const uint32_t _length,const uint32_t access)
{
  buffer_t & buffer = buffer_t::storage().at(buffer_name);
  const uint32_t length = (_length == 0) ? buffer.size : _length;

  if(buffer.mapped != 0) {
    RSXGL_ERROR(GL_INVALID_OPERATION,0);
  }

  int rsx_access = rsxgl_buffer_access(access);
  if(rsx_access == ~0) {
    RSXGL_ERROR(GL_INVALID_ENUM,0);
  }

  if(offset < 0 || length < 0) {
    RSXGL_ERROR(GL_INVALID_VALUE,0);
  }
  if(!rsxgl_buffer_valid_range(buffer,offset,length)) {
    RSXGL_ERROR(GL_INVALID_VALUE,0);
  }

  // TODO - replace this with something smarter that doesn't conservatively decide to block on the GPU:
  if(buffer.timestamp > 0) {
    rsxgl_timestamp_wait(ctx,buffer.timestamp);
    buffer.timestamp = 0;
  }

  // 
  buffer.mapped = rsx_access;
  buffer.mapped_offset = 0;
  buffer.mapped_size = buffer.size;

  RSXGL_NOERROR(rsxgl_arena_address(memory_arena_t::storage().at(buffer.arena),buffer.memory));
}

//
GLAPI void* APIENTRY
glMapBuffer (GLenum target, GLenum access)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR(GL_INVALID_ENUM,0);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR(GL_INVALID_OPERATION,0);
  }
  
  return rsxgl_map_buffer_range(ctx,ctx -> buffer_binding.names[rsx_target],0,0,access);
}

GLAPI GLvoid* APIENTRY
glMapBufferRange (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR(GL_INVALID_ENUM,0);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR(GL_INVALID_OPERATION,0);
  }

  return rsxgl_map_buffer_range(ctx,ctx -> buffer_binding.names[rsx_target],offset,length,access);
}

GLAPI void APIENTRY
glFlushMappedBufferRange (GLenum target, GLintptr offset, GLsizeiptr length)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }
  buffer_t & buffer = ctx -> buffer_binding[rsx_target];

  if(buffer.mapped == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(offset < 0 || length < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  if(!rsxgl_buffer_valid_range(buffer,offset,length)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  // TODO - Implement this function.

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glUnmapBuffer (GLenum target)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR(GL_INVALID_ENUM,GL_FALSE);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR(GL_INVALID_OPERATION,0);
  }
  buffer_t & buffer = ctx -> buffer_binding[rsx_target];

  if(buffer.mapped == 0) {
    RSXGL_ERROR(GL_INVALID_OPERATION,GL_FALSE);
  }

  buffer.mapped = 0;
  buffer.mapped_offset = 0;
  buffer.mapped_size = 0;

  RSXGL_NOERROR(GL_TRUE);
}

GLAPI void APIENTRY
glGetBufferParameteriv (GLenum target, GLenum pname, GLint *params)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }
  buffer_t & buffer = ctx -> buffer_binding[rsx_target];

  if(pname == GL_BUFFER_SIZE) {
    *params = buffer.size;
  }
  else if(pname == GL_BUFFER_USAGE) {
    if(buffer.usage == RSXGL_STREAM_DRAW) {
      *params = GL_STREAM_DRAW;
    }
    else if(buffer.usage == RSXGL_STREAM_READ) {
      *params = GL_STREAM_READ;
    }
    else if(buffer.usage == RSXGL_STREAM_COPY) {
      *params = GL_STREAM_COPY;
    }
    else if(buffer.usage == RSXGL_STATIC_DRAW) {
      *params = GL_STATIC_DRAW;
    }
    else if(buffer.usage == RSXGL_STATIC_READ) {
      *params = GL_STATIC_READ;
    }
    else if(buffer.usage == RSXGL_STATIC_COPY) {
      *params = GL_STATIC_COPY;
    }
    else if(buffer.usage == RSXGL_DYNAMIC_DRAW) {
      *params = GL_DYNAMIC_DRAW;
    }
    else if(buffer.usage == RSXGL_DYNAMIC_READ) {
      *params = GL_DYNAMIC_READ;
    }
    else if(buffer.usage == RSXGL_DYNAMIC_COPY) {
      *params = GL_DYNAMIC_COPY;
    }
  }
  else if(pname == GL_BUFFER_ACCESS) {
    if(buffer.mapped == RSXGL_READ_ONLY) {
      *params = GL_READ_ONLY;
    }
    else if(buffer.mapped == RSXGL_WRITE_ONLY) {
      *params = GL_WRITE_ONLY;
    }
    else if(buffer.mapped == RSXGL_READ_WRITE) {
      *params = GL_READ_WRITE;
    }
    else {
      *params = 0;
    }
  }
  else if(pname == GL_BUFFER_ACCESS_FLAGS) {
    *params = (buffer.mapped & RSXGL_READ_ONLY ? GL_MAP_READ_BIT : 0) | (buffer.mapped & RSXGL_WRITE_ONLY ? GL_MAP_WRITE_BIT : 0);
  }
  else if(pname == GL_BUFFER_MAPPED) {
    *params = (buffer.mapped != 0) ? GL_TRUE : GL_FALSE;
  }
  else if(pname == GL_BUFFER_MAP_OFFSET) {
    *params = buffer.mapped_offset;
  }
  else if(pname == GL_BUFFER_MAP_LENGTH) {
    *params = buffer.mapped_size;
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetBufferPointerv (GLenum target, GLenum pname, GLvoid** params)
{
  const size_t rsx_target = rsxgl_buffer_target(target);
  if(rsx_target == ~0U) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }
  buffer_t & buffer = ctx -> buffer_binding[rsx_target];

  if(pname == GL_BUFFER_MAP_POINTER) {
    if(buffer.mapped != 0) {
      *params = rsxgl_arena_address(memory_arena_t::storage().at(buffer.arena),buffer.memory);
    }
    else {
      *params = 0;
    }
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glCopyBufferSubData (GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
  if(readOffset < 0 || writeOffset < 0 || size < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  int
    iread = rsxgl_buffer_target(readTarget),
    iwrite = rsxgl_buffer_target(writeTarget);

  if(iread < 0 || iwrite < 0) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> buffer_binding.names[iread] == 0 || ctx -> buffer_binding.names[iwrite] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  buffer_t
    & read_buffer = ctx -> buffer_binding[iread],
    & write_buffer = ctx -> buffer_binding[iwrite];

  const memory_t
    srcmem = read_buffer.memory,
    dstmem = write_buffer.memory;

  if((readOffset + size) > read_buffer.size ||
     (writeOffset + size) > write_buffer.size) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  
  const uint32_t timestamp = rsxgl_timestamp_create(ctx,1);

  gcmContextData * context = ctx -> gcm_context();

  uint32_t * buffer = gcm_reserve(context,12);

  gcm_emit_channel_method_at(buffer,0,1,0x184,2);
  gcm_emit_at(buffer,1,RSXGL_TRANSFER_LOCATION(srcmem.location));
  gcm_emit_at(buffer,2,RSXGL_TRANSFER_LOCATION(dstmem.location));

  gcm_emit_channel_method_at(buffer,3,1,0x30c,8);
  gcm_emit_at(buffer,4,srcmem.offset + readOffset);
  gcm_emit_at(buffer,5,dstmem.offset + writeOffset);
  gcm_emit_at(buffer,6,size);
  gcm_emit_at(buffer,7,size);
  gcm_emit_at(buffer,8,size);
  gcm_emit_at(buffer,9,1);
  gcm_emit_at(buffer,10,((u32)1 << 8) | 1);
  gcm_emit_at(buffer,11,0);

  gcm_finish_n_commands(context,12);

  rsxgl_timestamp_post(ctx,timestamp);

  rsxgl_assert(timestamp >= ctx -> buffer_binding[iread].timestamp);
  rsxgl_assert(timestamp >= ctx -> buffer_binding[iwrite].timestamp);

  ctx -> buffer_binding[iread].timestamp = timestamp;
  ctx -> buffer_binding[iwrite].timestamp = timestamp;

  RSXGL_NOERROR_();
}

void
rsxgl_buffer_validate(rsxgl_context_t *,buffer_t & buffer,const uint32_t start,const uint32_t length,const uint32_t timestamp)
{
  rsxgl_assert(timestamp >= buffer.timestamp);
  buffer.timestamp = timestamp;

  if(buffer.invalid) {
    // TODO - here will go flushing of mapped buffers, to replace the current scheme where the CPU synchronizes with the GPU.

    buffer.invalid = 1;
  }
}
