// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// sync.cc - Implement the glFlush and glFinish functions, and OpenGL synchronization objects.

#include <GL3/gl3.h>

#include "gl_fifo.h"
#include "rsxgl_assert.h"
#include "debug.h"
#include "error.h"
#include "rsxgl_context.h"
#include "state.h"
#include "program.h"
#include "attribs.h"
#include "uniforms.h"
#include "sync.h"

#include "gl_object.h"

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

static inline void
rsxgl_flush(rsxgl_context_t * ctx)
{
  rsxgl_gcm_flush(ctx -> gcm_context());
}

GLAPI void APIENTRY
glFlush (void)
{
  rsxgl_flush(current_ctx());

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glFinish (void)
{
  rsxgl_context_t * ctx = current_ctx();

  // TODO - Rumor has it that waiting on ctx -> ref is "slow". See if this is unacceptable, and see if a sync object is any better.
  const uint32_t ref = ctx -> ref++;
  rsxgl_emit_set_ref(ctx -> gcm_context(),ref);
  rsxgl_flush(ctx);

  gcmControlRegister volatile *control = gcmGetControlRegister();

  __sync();

  const useconds_t timeout = RSXGL_SYNC_SLEEP_INTERVAL * RSXGL_FINISH_SLEEP_ITERATIONS;
  const useconds_t timeout_interval = RSXGL_SYNC_SLEEP_INTERVAL;

  // Wait some interval for the GPU to finish:
  if(timeout > 0) {
    useconds_t remaining_timeout = timeout;
    while(control -> ref != ref && remaining_timeout > 0) {
      if(timeout_interval > 0) {
	usleep(timeout_interval);
	remaining_timeout -= timeout_interval;
      }
    }
  }
  // Wait forever:
  else {
    while(control -> ref != ref) {
      if(timeout_interval > 0) {
	usleep(timeout_interval);
      }
    }
  }

  RSXGL_NOERROR_();
}

// Dole out RSX semaphore indices
typedef name_space< RSXGL_MAX_SYNC_OBJECTS, true > rsxgl_sync_object_name_space_type;

static rsxgl_sync_object_name_space_type &
rsxgl_sync_object_name_space()
{
  static rsxgl_sync_object_name_space_type name_space;
  return name_space;
}

rsxgl_sync_object_index_type
rsxgl_sync_object_allocate()
{
  std::pair< rsxgl_sync_object_name_space_type::name_type, bool > tmp = rsxgl_sync_object_name_space().create_name();
  if(tmp.second) {
    return tmp.first + 64;
  }
  else {
    return RSXGL_MAX_SYNC_OBJECTS;
  }
}

void
rsxgl_sync_object_free(const rsxgl_sync_object_index_type index)
{
  const rsxgl_sync_object_index_type tmp = index - 64;
  if(tmp < RSXGL_MAX_SYNC_OBJECTS) {
    rsxgl_sync_object_name_space().destroy_name(tmp);
  }
}

// Sync objects are not considered true "GL objects," but they do require library-generated names.
// So we re-use that capability from gl_object<>. But since they can't be bound or orphaned, etc.,
// this class does not use the CRTP the way that other GL objects do.
struct rsxgl_sync_object_t {
  typedef gl_object< rsxgl_sync_object_t, RSXGL_MAX_SYNC_OBJECTS > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;

  static storage_type & storage();

  name_type name;
  rsxgl_sync_object_index_type index;
  uint32_t status:1, value:31;

  rsxgl_sync_object_t()
    : name(0), index(RSXGL_MAX_SYNC_OBJECTS), status(0), value(0) {
  }
};

rsxgl_sync_object_t::storage_type &
rsxgl_sync_object_t::storage()
{
  static rsxgl_sync_object_t::storage_type _storage(RSXGL_MAX_SYNC_OBJECTS);
  return _storage;
}

static const uint32_t rsxgl_sync_token_max = (1 << 23);
static const uint32_t RSXGL_SYNC_UNSIGNALED_TOKEN = (1 << 24);
static uint32_t _rsxgl_sync_token = 0;

static uint32_t
rsxgl_sync_token()
{
  return (_rsxgl_sync_token = (_rsxgl_sync_token + 1) & rsxgl_sync_token_max);
}

GLAPI GLsync APIENTRY
glFenceSync (GLenum condition, GLbitfield flags)
{
  if(condition != GL_SYNC_GPU_COMMANDS_COMPLETE) {
    RSXGL_ERROR(GL_INVALID_ENUM,0);
  }

  if(flags != 0) {
    RSXGL_ERROR(GL_INVALID_VALUE,0);
  }

  const rsxgl_sync_object_t::name_type name = rsxgl_sync_object_t::storage().create_name_and_object();

  const rsxgl_sync_object_index_type index = rsxgl_sync_object_allocate();
  const uint32_t token = rsxgl_sync_token();

  if(index != RSXGL_MAX_SYNC_OBJECTS) {
    rsxgl_sync_object_t * sync_object = &rsxgl_sync_object_t::storage().at(name);
    sync_object -> name = name;
    sync_object -> status = 0;
    sync_object -> index = index;
    sync_object -> value = token;
  
    rsxgl_sync_cpu_signal(index,RSXGL_SYNC_UNSIGNALED_TOKEN);
    rsxgl_emit_sync_gpu_signal_read(current_ctx() -> base.gcm_context,sync_object -> index,token);

    RSXGL_NOERROR((GLsync)sync_object);
  }
  else {
    RSXGL_ERROR(GL_OUT_OF_MEMORY,0);
  }
}

GLAPI GLboolean APIENTRY
glIsSync (GLsync sync)
{
  return (sync != 0) && (rsxgl_sync_object_t::storage().is_object(reinterpret_cast< rsxgl_sync_object_t * >(sync) -> name));
}

GLAPI void APIENTRY
glDeleteSync (GLsync sync)
{
  if(sync == 0 || !rsxgl_sync_object_t::storage().is_object(reinterpret_cast< rsxgl_sync_object_t * >(sync) -> name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  rsxgl_sync_object_t * sync_object = reinterpret_cast< rsxgl_sync_object_t * >(sync);

  if(sync_object -> index != RSXGL_MAX_SYNC_OBJECTS) {
    rsxgl_sync_object_free(sync_object -> index);
  }

  rsxgl_sync_object_t::storage().destroy(sync_object -> name);
}

GLAPI GLenum APIENTRY
glClientWaitSync (GLsync sync, GLbitfield flags, GLuint64 timeout)
{
  if(sync == 0 || !rsxgl_sync_object_t::storage().is_object(reinterpret_cast< rsxgl_sync_object_t * >(sync) -> name)) {
    RSXGL_ERROR(GL_INVALID_VALUE,GL_WAIT_FAILED);
  }

  static const GLbitfield valid_flags = GL_SYNC_FLUSH_COMMANDS_BIT;
  if((flags & ~valid_flags) != 0) {
    RSXGL_ERROR(GL_INVALID_VALUE,GL_WAIT_FAILED);
  }

  rsxgl_context_t * ctx = current_ctx();

  rsxgl_sync_object_t * sync_object = reinterpret_cast< rsxgl_sync_object_t * >(sync);

  // Flush it all:
  if(flags & GL_SYNC_FLUSH_COMMANDS_BIT) {
    rsxgl_flush(ctx);
  }

  // Maybe it's already been set?
  if(sync_object -> status) {
    RSXGL_NOERROR(GL_ALREADY_SIGNALED);
  }

  // timeout is nanoseconds - convert to microseconds:
  const useconds_t timeout_usec = timeout / 1000;

  const int result = rsxgl_sync_cpu_wait(sync_object -> index,sync_object -> value,timeout_usec,RSXGL_SYNC_SLEEP_INTERVAL);

  if(result) {
    sync_object -> status = 1;
    RSXGL_NOERROR(GL_CONDITION_SATISFIED);
  }
  else {
    RSXGL_NOERROR(GL_TIMEOUT_EXPIRED);
  }
}

GLAPI void APIENTRY
glWaitSync (GLsync sync, GLbitfield flags, GLuint64 timeout)
{
  if(sync == 0 || !rsxgl_sync_object_t::storage().is_object(reinterpret_cast< rsxgl_sync_object_t * >(sync) -> name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(flags != 0 || timeout != GL_TIMEOUT_IGNORED) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  // TODO - Do something here. Not sure what.

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetSynciv (GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values)
{
  if(sync == 0 || !rsxgl_sync_object_t::storage().is_object(reinterpret_cast< rsxgl_sync_object_t * >(sync) -> name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(bufSize < 1) {
    if(length != 0) *length = 0;
    RSXGL_NOERROR_();
  }

  rsxgl_sync_object_t * sync_object = reinterpret_cast< rsxgl_sync_object_t * >(sync);

  if(pname == GL_OBJECT_TYPE) {
    *values = GL_SYNC_FENCE;
  }
  else if(pname == GL_SYNC_STATUS) {
    *values = (sync_object -> status) ? GL_SIGNALED : GL_UNSIGNALED;
  }
  else if(pname == GL_SYNC_CONDITION) {
    *values = GL_SYNC_GPU_COMMANDS_COMPLETE;
  }
  else if(pname == GL_SYNC_FLAGS) {
    *values = 0;
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(length != 0) *length = 1;

  RSXGL_NOERROR_();
}
