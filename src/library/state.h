// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// state.h - OpenGL state that's unrelated to any GL objects (e.g., blend settings).

#ifndef rsxgl_state_H
#define rsxgl_state_H

#include "gcm.h"
#include "gl_constants.h"
#include "gl_types.h"
#include "rsxgl_context.h"
#include "program.h"

#include <cstddef>
#include <bitset>
#include <boost/integer.hpp>

enum compare_funcs {
  RSXGL_NEVER = 0,
  RSXGL_LESS = 1,
  RSXGL_EQUAL = 2,
  RSXGL_LEQUAL = 3,
  RSXGL_GREATER = 4,
  RSXGL_NOTEQUAL = 5,
  RSXGL_GEQUAL = 6,
  RSXGL_ALWAYS = 7
};

enum blend_funcs {
  RSXGL_ZERO = 0,
  RSXGL_ONE = 1,
  RSXGL_SRC_COLOR = 2,
  RSXGL_ONE_MINUS_SRC_COLOR = 3,
  RSXGL_SRC_ALPHA = 4,
  RSXGL_ONE_MINUS_SRC_ALPHA = 5,
  RSXGL_DST_ALPHA = 6,
  RSXGL_ONE_MINUS_DST_ALPHA = 7,
  RSXGL_DST_COLOR = 8,
  RSXGL_ONE_MINUS_DST_COLOR = 9,
  RSXGL_SRC_ALPHA_SATURATE = 10,
  RSXGL_CONSTANT_COLOR = 11,
  RSXGL_ONE_MINUS_CONSTANT_COLOR = 12,
  RSXGL_CONSTANT_ALPHA = 13,
  RSXGL_ONE_MINUS_CONSTANT_ALPHA = 14
};

enum blend_equations {
  RSXGL_FUNC_ADD = 0,
  RSXGL_FUNC_MIN = 1,
  RSXGL_FUNC_MAX = 2,
  RSXGL_FUNC_SUBTRACT = 3,
  RSXGL_FUNC_REVERSE_SUBTRACT = 4
};

enum stencil_ops {
  //RSXGL_ZERO = 0,
  RSXGL_INVERT = 1,
  RSXGL_KEEP = 2,
  RSXGL_REPLACE = 3,
  RSXGL_INCR = 4,
  RSXGL_DECR = 5,
  RSXGL_INCR_WRAP = 6,
  RSXGL_DECR_WRAP = 7
};

enum polygon_mode {
  RSXGL_POLYGON_MODE_POINT = 0,
  RSXGL_POLYGON_MODE_LINE = 1,
  RSXGL_POLYGON_MODE_FILL = 2
};

enum cull_face {
  RSXGL_CULL_FRONT = 0,
  RSXGL_CULL_BACK = 1,
  RSXGL_CULL_FRONT_AND_BACK = 2
};

enum face {
  RSXGL_FACE_CW = 0,
  RSXGL_FACE_CCW = 1
};

enum pixel_store_alignment {
  RSXGL_PIXEL_STORE_ALIGNMENT_1 = 0,
  RSXGL_PIXEL_STORE_ALIGNMENT_2 = 1,
  RSXGL_PIXEL_STORE_ALIGNMENT_4 = 2,
  RSXGL_PIXEL_STORE_ALIGNMENT_8 = 3
};

struct state_t {
  union {
    uint32_t all;
    struct {
      uint32_t viewport:1,
	depth_range:1,
	scissor:1,
	color_write_mask:1,
	depth_write_mask:1,
	primitive_restart:1,
	the_rest:1,
	framebuffer:1,
	program:1;
    } parts;
  } invalid;

  struct {
    uint32_t blend:1, scissor:1, depth_test:1, primitive_restart:1;
  } enable;

  struct {
    uint64_t x:16,y:16,width:16,height:16;
    uint8_t cullNearFar:1, cullIgnoreW: 1, clampZ:1;
    float depthRange[2];
  } viewport;

  struct {
    uint64_t x:16,y:16,width:16,height:16;
  } scissor;

  struct {
    uint8_t r:1, g:1, b:1, a:1, depth:1;
  } write_mask;

  struct {
    uint32_t clear;
  } color;

  struct {
    uint32_t func:3,clear:24;
  } depth;

  struct {
    uint32_t src_rgb_func:4, src_alpha_func:4, dst_rgb_func:4, dst_alpha_func:4,
      rgb_equation:3, alpha_equation:3;
    uint32_t color;
  } blend;

  struct {
    uint8_t clear;
    
    struct {
      uint32_t enable:1, ref:8, mask:8, writemask:8;
      uint32_t func:3, fail_op:3, zfail_op:3, pass_op:3;
    } face[2];

  } stencil;

  struct {
    uint32_t cullEnable:1, cullFace:2, frontFace:1, frontMode: 2, backMode: 2
      ;
    float offsetFactor, offsetUnits;    
  } polygon;

  struct {
    uint32_t pack_swap_bytes:1, pack_lsb_first:1, pack_alignment:2, unpack_swap_bytes:1, unpack_lsb_first:1, unpack_alignment:2;
    uint32_t pack_row_length, pack_image_height, pack_skip_pixels, pack_skip_rows, pack_skip_images;
    uint32_t unpack_row_length, unpack_image_height, unpack_skip_pixels, unpack_skip_rows;
  } pixel_store;

  float lineWidth;
  float pointSize;
  uint32_t primitiveRestartIndex;

  state_t();
};

struct rsxgl_context_t;

void rsxgl_state_validate(rsxgl_context_t *);

#endif
