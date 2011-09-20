/*
 * rsxgltest - clear
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "sine_wave.h"

char * rsxgltest_name = "clearscreen";

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
  glClearColor(1,1,0,1);
}

int
rsxgltest_draw()
{
  float rgb[3] = {
    compute_sine_wave(rgb_waves,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 1,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 2,rsxgltest_elapsed_time)
  };

  glClearColor(rgb[0],rgb[1],rgb[2],1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  return 1;
}

void
rsxgltest_exit()
{
}
