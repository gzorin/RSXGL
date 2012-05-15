#ifndef gl_fifo_H
#define gl_fifo_H

#include <rsx/gcm_sys.h>

#include "rsxgl_assert.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t __attribute__((noinline)) gcm_reserve_callback(gcmContextData *,uint32_t);

// TODO - Compile-time option to make the command buffer length checking in gcm_reserve
// a no-op. The application would need to make sure that it creates an adequately-sized
// command buffer that is flushed regularly.
static inline uint32_t *
gcm_reserve(gcmContextData * context,const uint32_t length)
{
  if((context -> current + length) > context -> end) {
    int32_t r = gcm_reserve_callback(context,length);
    rsxgl_assert(r == 0);
  }
  return context -> current;
}

static inline void
gcm_emit(uint32_t ** buffer,const uint32_t word)
{
  **buffer = word;
  *buffer = *buffer + 1;
}

#define GCM_MAX_METHOD_ARGS 2047

static inline void
gcm_emit_method(uint32_t ** buffer,const uint32_t method,const uint32_t n)
{
  gcm_emit(buffer,method | (n << 18));
}

static inline void
gcm_emit_channel_method(uint32_t ** buffer,const uint32_t channel,const uint32_t method,const uint32_t n)
{
  gcm_emit(buffer,method | (n << 18) | (channel << 13));
}

static inline void
gcm_emit_at(uint32_t * buffer,const uint32_t location,const uint32_t word)
{
  buffer[location] = word;
}

static inline void
gcm_emit_method_at(uint32_t * buffer,const uint32_t location,const uint32_t method,const uint32_t n)
{
  gcm_emit_at(buffer,location,method | (n << 18));
}

static inline void
gcm_emit_channel_method_at(uint32_t * buffer,const uint32_t location,const uint32_t channel,const uint32_t method,const uint32_t n)
{
  gcm_emit_at(buffer,location,method | (n << 18) | (channel << 13));
}

static inline void
gcm_emit_nop_at(uint32_t * buffer,const uint32_t location,const uint32_t n)
{
  gcm_emit_method_at(buffer,location,0x100,n);
}

static inline void
gcm_emit_wait_for_idle_at(uint32_t * buffer,const uint32_t location,const uint32_t n)
{
  gcm_emit_method_at(buffer,location,0x110,n);
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

static inline uint32_t
gcm_begin_list(gcmContextData * context)
{
  // Calculate the offset that gets passed to a "call" method:
  uint32_t call_offset = 0;
  int32_t s = gcmAddressToOffset(context -> current + 1,&call_offset);
  rsxgl_assert(s == 0);

  // Add a nop - this gets replaced by gcm_finish_list with a "jump" method:
  uint32_t * buffer = gcm_reserve(context,1);
  gcm_emit_at(buffer,0,0);
  gcm_finish_n_commands(context,1);
}

static inline void
gcm_finish_list(gcmContextData * context,const uint32_t call_offset,const bool call)
{
  // Calculate the offset to "jump" to, in order to skip a command list that may or may not be called right now.
  uint32_t jump_offset = 0;
  int32_t s = gcmAddressToOffset(context -> current,&jump_offset);
  rsxgl_assert(s == 0);

  // Replace the nop added by gcm_begin_list with a "jump" to jump_offset:
  uint32_t * buffer = context -> current - (jump_offset - call_offset) - 1;
  gcm_emit_at(buffer,0,gcm_jump_cmd(jump_offset));

  // Insert the "return" method, and optionally a "call" method to invoke the list immediately:
  const uint32_t n = call ? 2 : 1;

  buffer = gcm_reserve(context,n);
  gcm_emit_at(buffer,0,gcm_return_cmd());

  if(call) {
    gcm_emit_at(buffer,1,gcm_call_cmd(call_offset));
  }

  gcm_finish_n_commands(context,n);
}

#ifdef __cplusplus
}
#endif

#endif
