#include "sine_wave.h"

float
compute_sine_wave(const struct sine_wave_t * wave,float t)
{
  return wave -> A * sinf(wave -> omega * t) + wave -> D;
}
