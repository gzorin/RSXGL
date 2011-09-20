/*
 * rsxgltest - draw
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "sine_wave.h"

#include <stddef.h>
typedef uint8_t u8;
typedef uint32_t u32;
#include "draw_vpo.h"
#include "draw_fpo.h"

char * rsxgltest_name = "draw";

extern void tcp_printf(const char * fmt,...);
extern void report_glerror(const char *);
extern void summarize_program(const char *,GLuint);
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

GLuint buffer = 0;
GLuint shaders[2] = { 0,0 };
GLuint program = 0;

void
rsxgltest_init(int argc,const char ** argv)
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  // Set up us the program:
  shaders[0] = glCreateShader(GL_VERTEX_SHADER);
  shaders[1] = glCreateShader(GL_FRAGMENT_SHADER);

  program = glCreateProgram();

  glAttachShader(program,shaders[0]);
  glAttachShader(program,shaders[1]);

  // Supply shader binaries:
  glShaderBinary(1,shaders,0,draw_vpo,draw_vpo_size);
  glShaderBinary(1,shaders + 1,0,draw_fpo,draw_fpo_size);

  // Link the program for real:
  glLinkProgram(program);
  glValidateProgram(program);

  summarize_program("draw",program);

  GLint 
    vertex_location = glGetAttribLocation(program,"inputvertex.vertex"),
    texcoord_location = glGetAttribLocation(program,"inputvertex.texcoord");
  tcp_printf("vertex_location: %i\n",vertex_location);
  tcp_printf("texcoord_location: %i\n",texcoord_location);

  glUseProgram(program);

  // Set up us the vertex data:
  float geometry[] = {
    0, 0, 0, 1,
    0,0,

    0.5, 0, 0, 1,
    1,0,

    0.0, 0.5, 0, 1,
    0,1,

    0.5, 0, 0, 1,
    1,0,

    0.5, 0.5, 0, 1,
    1,1,

    0.0, 0.5, 0, 1,
    0,1
  };

  glGenBuffers(1,&buffer);
  glBindBuffer(GL_ARRAY_BUFFER,buffer);
  glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 6 * 6,geometry,GL_STATIC_DRAW);
  glEnableVertexAttribArray(vertex_location);
  glEnableVertexAttribArray(texcoord_location);
  glVertexAttribPointer(vertex_location,4,GL_FLOAT,GL_FALSE,sizeof(float) * 6,0);
  glVertexAttribPointer(texcoord_location,2,GL_FLOAT,GL_FALSE,sizeof(float) * 6,(const GLvoid *)(sizeof(float) * 4));

  glBindBuffer(GL_ARRAY_BUFFER,0);
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

  glDrawArrays(GL_TRIANGLES,0,6);

  return 1;
}

void
rsxgltest_exit()
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  glDeleteShader(shaders[0]);
  glDeleteProgram(program);
  glDeleteShader(shaders[1]);
}
