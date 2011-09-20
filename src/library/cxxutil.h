//-*-C++-*-

#ifndef rsxgl_cxxutil_H
#define rsxgl_cxxutil_H

#include <stdint.h>

template< typename Int, Int Align >
Int align_pot(const Int value)
{
  const Int value_mod_align = value & (Align - 1);
  return (value_mod_align != 0) ? (value + Align - value_mod_align) : value;
}

// From "Bit Twiddling Hacks"
// by Sean Eron Anderson
// http://graphics.stanford.edu/~seander/bithacks.html
template< typename Int >
bool is_pot(const Int v)
{
  return v && !(v & (v - 1));
}

static inline uint32_t
log2_uint32(const uint32_t value)
{
  uint32_t v = value;         // 32-bit value to find the log2 of 
  register uint32_t r; // result of log2(v) will go here
  register uint32_t shift;

  r =     (v > 0xFFFF) << 4; v >>= r;
  shift = (v > 0xFF  ) << 3; v >>= shift; r |= shift;
  shift = (v > 0xF   ) << 2; v >>= shift; r |= shift;
  shift = (v > 0x3   ) << 1; v >>= shift; r |= shift;
  r |= (v >> 1);

  return r;
}

#endif
