#ifndef rsxgltest_H
#define rsxgltest_H

#include <io/pad.h>

#if defined(__cplusplus)
extern "C" {
#endif

void tcp_printf(const char * fmt,...);
void report_glerror(const char *);
void summarize_program(const char *,GLuint);
extern int rsxgltest_width, rsxgltest_height;
extern float rsxgltest_elapsed_time, rsxgltest_last_time, rsxgltest_delta_time;

#if defined(__cplusplus)
}
#endif

#endif
