//-*-C-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// rsxgl.h - RSXGL-specific API

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#ifndef RSXGL_H
#define RSXGL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Set shared-memory size and command buffer length: */
struct rsxgl_init_parameters_t {
  khronos_usize_t gcm_buffer_size;
  khronos_usize_t command_buffer_length;
  uint32_t max_swap_wait_iterations;
  useconds_t swap_wait_interval;
};

/*! \brief Customize the resources that RSXGL allocates upon initialization. Call this, optionally, before
 * making any EGL or GL3 function calls.
 */
EGLAPI void EGLAPIENTRY rsxglInit(struct rsxgl_init_parameters_t const *);

#ifdef __cplusplus
}
#endif

#endif
