/*
 * rsxgltest - viewport & scissor test
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "sine_wave.h"

char * rsxgltest_name = "viewport_scissor";

extern float rsxgltest_elapsed_time;

struct sine_wave_t rgb_waves[3] = {
  {
    .A = 0.5f,
    .D = 0.5f,
    .omega = 1.0f
  },
  {
    .A = 0.5f,
    .D = 0.5f,
    .omega = 1.5f
  },
  {
    .A = 0.5f,
    .D = 0.5f,
    .omega = 2.5f
  }
};

void
rsxgltest_init(int argc,const char ** argv)
{
}

int
rsxgltest_draw()
{
  glDisable(GL_SCISSOR_TEST);
  glViewport(0,0,1920,1080);

  float rgb[3] = {
    compute_sine_wave(rgb_waves,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 1,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 2,rsxgltest_elapsed_time)
  };

  glClearColor(rgb[0],rgb[1],rgb[2],1.0);

  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_SCISSOR_TEST);

  glScissor(50,50,885,465);
  glViewport(50,50,885,465);
  glClearColor(1,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT);

  glScissor(985,50,885,465);
  glViewport(985,50,885,465);
  glClearColor(0,1,0,1);
  glClear(GL_COLOR_BUFFER_BIT);

  glScissor(50,565,885,465);
  glViewport(50,565,885,465);
  glClearColor(0,0,1,1);
  glClear(GL_COLOR_BUFFER_BIT);

  glScissor(985,565,885,465);
  glViewport(985,565,885,465);
  glClearColor(0,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT);

  return 1;
}

void
rsxgltest_exit()
{
}
