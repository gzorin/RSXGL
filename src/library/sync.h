//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// sync.h - Private constants, types and functions related to synchronizing the CPU and the RSX.

#ifndef rsxgl_sync_H
#define rsxgl_sync_H

#include "nv40.h"
#include "gcm.h"
#include "gl_fifo.h"
#include "rsxgl_assert.h"
#include "cxxutil.h"

#include <sys/unistd.h>
#include <algorithm>
#include <stddef.h>
#include <ppu_intrinsics.h>

//
static inline void
rsxgl_gcm_flush(gcmContextData * context)
{
  uint32_t offset;
  gcmControlRegister volatile *control = gcmGetControlRegister();

  __sync();
  
  gcmAddressToOffset(context -> current, &offset);
  control->put = offset;
}

// Insert a command to set the RSX's reference register to something:
static inline void
rsxgl_emit_set_ref(gcmContextData * context,const uint32_t value)
{
  uint32_t * buffer = gcm_reserve(context,2);

  gcm_emit_method_at(buffer,0,0x50,1);
  gcm_emit_at(buffer,1,value);

  gcm_finish_n_commands(context,2);
}

// Set a sync object to some value. If the RSX is waiting for this value, then it'll wake up and go.
static inline void
rsxgl_sync_cpu_signal(const uint8_t index,const uint32_t value)
{
  volatile uint32_t * object = gcmGetLabelAddress(index);
  rsxgl_assert(object != 0);
  *object = value;
}

// Get the value of a sync object:
static inline uint32_t
rsxgl_sync_value(const uint8_t index)
{
  volatile uint32_t * object = gcmGetLabelAddress(index);
  rsxgl_assert(object != 0);
  return *object;
}

// Insert a command that will set sync object index to value once read processing is finished.
// (Equivalent to PSL1GHT's rsxSetWriteCommandLabel)
static inline void
rsxgl_emit_sync_gpu_signal_read(gcmContextData * context,const uint8_t index,const uint32_t value)
{
  uint32_t * buffer = gcm_reserve(context,4);

  gcm_emit_method_at(buffer,0,NV406ETCL_SEMAPHORE_OFFSET,1);
  gcm_emit_at(buffer,1,index << 4);
  gcm_emit_method_at(buffer,2,NV406ETCL_SEMAPHORE_RELEASE,1);
  gcm_emit_at(buffer,3,value);

  gcm_finish_n_commands(context,4);
}

// Insert a command that will set sync object index to value once write processing is finished.
// (Equivalent to PSL1GHT's rsxSetWriteBackendLabel)
static inline void
rsxgl_emit_sync_gpu_signal_write(gcmContextData * context,const uint8_t index,const uint32_t value)
{
  uint32_t * buffer = gcm_reserve(context,4);

  gcm_emit_method_at(buffer,0,NV40TCL_SEMAPHORE_OFFSET,1);
  gcm_emit_at(buffer,1,index << 4);
  gcm_emit_method_at(buffer,2,NV40TCL_SEMAPHORE_BACKENDWRITE_RELEASE,1);
  gcm_emit_at(buffer,3,(value&0xff00ff00) | ((value>>16)&0xff) | ((value&0xff)<<16));

  gcm_finish_n_commands(context,4);
}

// Block the CPU for a specified interval until the sync object is set to a specific value by the GPU.
// Timing will be inaccurate - instead of actually measuring how long the operation's taken, just
// divides desired timeout by timeout_interval, and run for that many iterations. Given that reading
// back the sync value is probably pretty slow, this function may run for longer than timeout.
// timeout_interval must therefore be some positive number - the function asserts that this is so.
//
// Returns 1 if the sync object was set to value while this function ran, 0 if it "timed out".
static inline int
rsxgl_sync_cpu_wait(const uint8_t index,const uint32_t value,const useconds_t timeout,const useconds_t timeout_interval)
{
  rsxgl_assert(timeout_interval > 0);

  volatile uint32_t * object = gcmGetLabelAddress(index);
  rsxgl_assert(object != 0);

  uint32_t current_value = *object;
  useconds_t i = 0;
  const useconds_t n = timeout / timeout_interval;

  for(;current_value != value && i < n;current_value = *object,++i) {
    usleep(timeout_interval);
  }

  return (current_value == value);
}
  
// Tell the GPU to wait until a sync object is set to some value:
// (Equivalent to PSL1GHT's rsxSetWaitLabel)
static inline void
rsxgl_sync_gpu_wait(gcmContextData * context,const uint8_t index,const uint32_t value)
{
  uint32_t * buffer = gcm_reserve(context,4);

  gcm_emit_method_at(buffer,0,NV406ETCL_SEMAPHORE_OFFSET,1);
  gcm_emit_at(buffer,1,0x10 * index);
  gcm_emit_method_at(buffer,2,NV406ETCL_SEMAPHORE_ACQUIRE,1);
  gcm_emit_at(buffer,3,value);

  gcm_finish_n_commands(context,4);  
}

// Return 0 is failure:
uint8_t rsxgl_sync_object_allocate();
void rsxgl_sync_object_free(uint8_t);

#endif
