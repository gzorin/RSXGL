#ifndef gl_fifo_H
#define gl_fifo_H

#include <rsx/gcm_sys.h>
#include <rsxgl_assert.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t __attribute__((noinline)) gcm_reserve_callback(gcmContextData *,uint32_t);

// TODO - Compile-time option to make the command buffer length checking in gcm_reserve
// a no-op. The application would need to make sure that it creates an adequately-sized
// command buffer that is flushed regularly.
static inline uint32_t *
gcm_reserve(gcmContextData * context,uint32_t length)
{
  if((context -> current + length) > context -> end) {
    int32_t r = gcm_reserve_callback(context,length);
    rsxgl_assert(r == 0);
  }
  return context -> current;
}

static inline void
gcm_emit(uint32_t ** buffer,uint32_t word)
{
  **buffer = word;
  *buffer = *buffer + 1;
}

#define GCM_MAX_METHOD_ARGS 2047

static inline void
gcm_emit_method(uint32_t ** buffer,uint32_t method,uint32_t n)
{
  gcm_emit(buffer,method | (n << 18));
}

static inline void
gcm_emit_channel_method(uint32_t ** buffer,uint32_t channel,uint32_t method,uint32_t n)
{
  gcm_emit(buffer,method | (n << 18) | (channel << 13));
}

static inline void
gcm_emit_at(uint32_t * buffer,uint32_t location,uint32_t word)
{
  buffer[location] = word;
}

static inline void
gcm_emit_method_at(uint32_t * buffer,uint32_t location,uint32_t method,uint32_t n)
{
  gcm_emit_at(buffer,location,method | (n << 18));
}

static inline void
gcm_emit_channel_method_at(uint32_t * buffer,uint32_t location,uint32_t channel,uint32_t method,uint32_t n)
{
  gcm_emit_at(buffer,location,method | (n << 18) | (channel << 13));
}

static inline void
gcm_finish_commands(gcmContextData * context,uint32_t ** buffer)
{
  context -> current = *buffer;
}

static inline void
gcm_finish_n_commands(gcmContextData * context,uint32_t n)
{
  context -> current += n;
}

static inline uint32_t
gcm_jump_cmd(const uint32_t offset)
{
  return 0x20000000 | offset;
}

static inline uint32_t
gcm_call_cmd(const uint32_t offset)
{
  return 0x00000002 | offset;
}

static inline uint32_t
gcm_return_cmd()
{
  return 0x00020000;
}

#ifdef __cplusplus
}
#endif

#endif
