/*
 * rsxgltest - scissor
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

char * rsxgltest_name = "scissor";

void
rsxgltest_init(int argc,const char ** argv)
{
}

int
rsxgltest_draw()
{
  glViewport(0,0,1920,1080);
  glClearColor(1,1,1,1);
  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_SCISSOR_TEST);

  glScissor(480,270,960,540);
  glClearColor(1,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT);

#if 0
  glScissor(985,50,885,465);
  glClearColor(0,1,0,1);
  glClear(GL_COLOR_BUFFER_BIT);

  glScissor(50,565,885,465);
  glClearColor(0,0,1,1);
  glClear(GL_COLOR_BUFFER_BIT);

  glScissor(985,565,885,465);
  glClearColor(0,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT);
#endif

#if 0
  //
  glScissor(240,135,480,270);
  glClearColor(0,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT);

  glScissor(240 + 960,135,480,270);
  glClearColor(0,0,1,1);
  glClear(GL_COLOR_BUFFER_BIT);

  glScissor(240,135 + 540,480,270);
  glClearColor(0,1,0,1);
  glClear(GL_COLOR_BUFFER_BIT);

  glScissor(240 + 960,135 + 540,480,270);
  glClearColor(1,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT);
#endif

  glDisable(GL_SCISSOR_TEST);

  return 1;
}

void
rsxgltest_exit()
{
}
