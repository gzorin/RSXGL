#include "pixel_store.h"

pixel_store_t::pixel_store_t()
  : swap_bytes(0),
    lsb_first(0),
    alignment(RSXGL_PIXEL_STORE_ALIGNMENT_4),
    row_length(0),
    image_height(0),
    skip_pixels(0),
    skip_rows(0),
    skip_images(0)
{ 
}

