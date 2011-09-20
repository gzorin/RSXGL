//-*-C-*-

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#ifndef RSXEGL_H
#define RSXEGL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Set shared-memory size and command buffer length: */
struct rsxegl_init_parameters_t {
  khronos_usize_t gcm_buffer_size;
  khronos_usize_t command_buffer_length;
};

EGLAPI void EGLAPIENTRY rsxeglInit(struct rsxegl_init_parameters_t const *);

/* Block main thread until the swap has finished: */
EGLAPI void EGLAPIENTRY rsxeglSwapSync(useconds_t,struct timeval *);

#ifdef __cplusplus
}
#endif

#endif
