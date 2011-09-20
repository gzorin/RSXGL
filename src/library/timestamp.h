//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// timestamp.h - GPU timestamping capability.
//
// The purpose of this mechanism is to allow the GPU to tell the CPU that it's done using
// certain objects, so that objects that if an object is glDelete*()'d, it'll be orphaned
// instead of destroyed immediately if the GPU is still using it. This is also meant to
// allow the CPU to block or otherwise delay operations that would modify an in-use object
// (such as glMapBuffer, or glTexSubImage, etc)
//
// These functions depend upon a sync object having been allocated for this purpose already.

#ifndef rsxgl_timestamp_H
#define rsxgl_timestamp_H

#include "sync.h"

// max_timestamp + 1 should be a power-of-two value.
// Should not return 0, because this is reserved for indicating that an object is not waiting
// on a GPU operation.

// Returns a tuple: the result, the next timestamp value, and a boolean if the 
#if 0
static inline uint32_t
rsxgl_timestamp_create(uint32_t & _next_timestamp,void (*overflow_callback)(void *),void * overflow_data)
{
  const uint32_t max_timestamp = RSXGL_MAX_TIMESTAMP;
  rsxgl_assert(is_pot(timestamp + 1));

  const uint32_t result = _next_timestamp;
  rsxgl_assert(result > 0);

  const uint32_t next_timestamp = _next_timestamp + 1;
  
  if(next_timestamp & ~max_timestamp || next_timestamp == 0) {
    if(overflow_callback) (*overflow_callback)(overflow_data);
    _next_timestamp = 1;
  }
  else {
    _next_timestamp = next_timestamp;
  }

  return result;
}
#endif

// Add the previously allocated timestamp to the command stream.
// See if a timestamp has been passed by the GPU:
static inline bool
rsxgl_timestamp_passed(uint32_t & cached_timestamp,const uint8_t index,const uint32_t compare)
{
  rsxgl_assert(index != 0);

  if(cached_timestamp < compare) {
    const uint32_t timestamp = rsxgl_sync_value(index);
    cached_timestamp = timestamp;
    return timestamp >= compare;
  }
  else {
    return true;
  }
}

// Conservative timestamp checking - only checks the "cached" timestamp, does not consult the GPU:
static inline bool
rsxgl_timestamp_passed_conservative(const uint32_t cached_timestamp,const uint32_t compare)
{
  return (cached_timestamp >= compare);
}

// Wait for the GPU to reach some timestamp. Returns true if the function did indeed need to wait,
// false otherwise.
static inline bool
rsxgl_timestamp_wait(uint32_t & cached_timestamp,const uint8_t index,const uint32_t compare,const useconds_t timeout_interval)
{
  if(cached_timestamp < compare) {
    volatile uint32_t * object = gcmGetLabelAddress(index);
    rsxgl_assert(object != 0);
    
    uint32_t timestamp = *object;

    for(;timestamp < compare;timestamp = *object) {
      usleep(timeout_interval);
    }

    cached_timestamp = timestamp;

    return true;
  }
  else {
    return false;
  }
}

#endif
