//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// pixel_store.h - Types and functions related to transfering blocks of pixels around.

#ifndef rsxgl_pixel_store_H
#define rsxgl_pixel_store_H

#include <stdint.h>

enum pixel_store_alignment {
  RSXGL_PIXEL_STORE_ALIGNMENT_1 = 0,
  RSXGL_PIXEL_STORE_ALIGNMENT_2 = 1,
  RSXGL_PIXEL_STORE_ALIGNMENT_4 = 2,
  RSXGL_PIXEL_STORE_ALIGNMENT_8 = 3
};

struct pixel_store_t {
  uint8_t swap_bytes:1,lsb_first:1,alignment;
  uint32_t row_length, image_height, skip_pixels, skip_rows, skip_images;

  pixel_store_t();
};

#endif
