//-*-C-*-

#ifndef rsxgl_types_H
#define rsxgl_types_H

#include "gl_constants.h"
#include "arena.h"
#include "buffer.h"

#include <stddef.h>
#include <gcm.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
struct surface_t {
  uint8_t surface, location;
  uint16_t pitch;
  uint32_t offset;
};

struct format_t {
  uint16_t enabled;
  uint16_t format;
  uint16_t width,height;
};
#endif

#ifdef __cplusplus
}
#endif

#endif
