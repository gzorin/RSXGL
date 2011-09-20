//-*-C-*-
#ifndef rsxgl_egl_internal_H
#define rsxgl_egl_internal_H

#include <stdint.h>
#include <EGL/egl.h>

#include "gcm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rsxegl_config_t {
  //
  EGLint egl_config_id;

  EGLint egl_buffer_size;
  EGLint egl_red_size, egl_blue_size, egl_green_size, egl_alpha_size;
  EGLint egl_depth_size, egl_stencil_size;

  // For allocating buffers, size in bytes:
  khronos_usize_t color_pixel_size, depth_pixel_size;

  // Gets passed to the RSX:
  uint8_t video_format;
  uint32_t format;
};

struct rsxegl_surface_t {
  // Is it double-buffered or not?
  EGLenum double_buffered;

  // Which buffer is current?
  uint32_t buffer;

  //
  uint32_t format;
  uint16_t width, height, x, y;
  uint32_t color_pitch, depth_pitch;
  uint32_t color_pixel_size, depth_pixel_size;

  // Address in RSX memory of the color and depth buffers:
  uint32_t color_addr[2], depth_addr;

  rsx_ptr_t _color_addr[2], _depth_addr;
};

enum rsxegl_context_callbacks {
  RSXEGL_MAKE_CONTEXT_CURRENT = 0,
  RSXEGL_POST_CPU_SWAP = 1,
  RSXEGL_POST_GPU_SWAP = 2,
  RSXEGL_DESTROY_CONTEXT = 3
};

struct rsxegl_context_t {
  int valid;
  
  EGLenum api;
  
  const struct rsxegl_config_t * config;
  const struct rsxegl_surface_t * draw, * read;
  gcmContextData * gcm_context;

  void (*callback)(struct rsxegl_context_t *,const uint8_t);
};

#ifdef __cplusplus
}
#endif

#endif
