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
  making any EGL or GL3 function calls.

  \param parameters Pointer to a parameters specification data strucuts. Its contents are copied by the function.
*/
void rsxglInit(struct rsxgl_init_parameters_t const * parameters);

/* The following functions are for compatibility with librsx - where librsx is
   used to do the setup that
   EGL usually performs.
*/
  
/*! \brief Create a new RSXGL context. Returns 0 on failure.
  \param gcm_context Pointer to a gcmContextData structure. The RSXGL context retains this pointer.
*/
void * rsxglCreateContext(void * gcm_context);
  
/*! \brief Set the format and location of the framebuffer to render into. This function does /not/
  perform the equivalent to rsxSetSurface - the application should do that before performing and OpenGL
  rendering - but simply tells the GL where its framebuffer is for the purpose of performing operations
  such as glCopyTexImage.
  \param context The RSXGL context to modify
  \param surface Pointer to a librsx gcmSurface data structure. Its contents are copied by this function.
  \param buffer Integer indicating if the surface is the back (0) or front (1) buffer.
*/
void rsxglSetSurface(void * context,void * surface,uint8_t buffer);

/* \brief Make an RSXGL context the current one. Should be called before performing any OpenGL operations.
   Accepts a NULL pointer, which will cause subsequent OpenGL operations to abort the program.
   \param context The RSXGL context to make current
*/
void rsxglMakeCurrent(void * context);

/* \brief Call this before performing the framebuffer flip, but after all OpenGL operations have been performed.
 */
void rsxglPreSwap();

/* \brief Call this after performing the framebuffer flip. It will cause post-rendering cleanup
   tasks, such as orphan cleanup, for the current context.
*/
void rsxglPostSwap();

/* \brief Destroy a RSXGL context.
   \param context The RSXGL context to make current
*/
void rsglDestroyContext(void * context);

#ifdef __cplusplus
}
#endif

#endif
