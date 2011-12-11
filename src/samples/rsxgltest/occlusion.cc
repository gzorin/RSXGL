/*
 * rsxgltest - occlusion
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#include <GL3/gl3ext.h>

#include "rsxgltest.h"
#include "math3d.h"
#include "sine_wave.h"

#include <stddef.h>
#include "occlusion_vpo.h"
#include "occlusion_fpo.h"

#include <io/pad.h>

#include <math.h>
#include <Eigen/Geometry>

const char * rsxgltest_name = "occlusion";

struct sine_wave_t rgb_waves[3] = {
  { 0.5f,
    0.5f,
    1.0f
  },
  {
    0.5f,
    0.5f,
    1.5f
  },
  {
    0.5f,
    0.5f,
    2.5f
  }
};

struct sine_wave_t xyz_waves[3] = {
  { 0.5f,
    0.5f,
    1.0f / 2.0f
  },
  {
    0.5f,
    0.5f,
    1.5f / 2.0f
  },
  {
    0.5f,
    0.5f,
    2.5f / 2.0f
  }
};

GLuint buffers[2] = { 0,0 };

GLuint shaders[2] = { 0,0 };
GLuint program = 0;

GLuint queries[2] = { 0,0 };

GLint ProjMatrix_location = -1, TransMatrix_location = -1, color_location = -1;

#define DTOR(X) ((X)*0.01745329f)
#define RTOD(d) ((d)*57.295788f)

Eigen::Projective3f ProjMatrix(perspective(DTOR(54.3),1920.0 / 1080.0,0.1,1000.0));

Eigen::Affine3f ViewMatrixInv = 
  lookat(Eigen::Vector3f(0,0,-10),
	 Eigen::Vector3f(0,0,0),
	 Eigen::Vector3f(0,1,0)).inverse();

extern "C"
void
rsxgltest_pad(unsigned int,const padData * paddata)
{
  if(paddata -> BTN_UP) {
    tcp_printf("up\n");
  }
  else if(paddata -> BTN_DOWN) {
    tcp_printf("down\n");
  }
}

extern "C"
void
rsxgltest_init(int argc,const char ** argv)
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  // Set up us the program:
  shaders[0] = glCreateShader(GL_VERTEX_SHADER);
  shaders[1] = glCreateShader(GL_FRAGMENT_SHADER);

  program = glCreateProgram();

  glAttachShader(program,shaders[0]);
  glAttachShader(program,shaders[1]);

  // Supply shader binaries:
  glShaderBinary(1,shaders,0,occlusion_vpo,occlusion_vpo_size);
  glShaderBinary(1,shaders + 1,0,occlusion_fpo,occlusion_fpo_size);

  // Link the program for real:
  glLinkProgram(program);
  glValidateProgram(program);
  
  summarize_program("draw",program);

  GLint 
    vertex_location = glGetAttribLocation(program,"position");

  ProjMatrix_location = glGetUniformLocation(program,"ProjMatrix");
  TransMatrix_location = glGetUniformLocation(program,"TransMatrix");
  color_location = glGetUniformLocation(program,"c");

  tcp_printf("vertex_location: %i\n",vertex_location);
  tcp_printf("ProjMatrix_location: %i TransMatrix_location: %i color_location: %i\n",
	     ProjMatrix_location,TransMatrix_location,color_location);

  glUseProgram(program);

  glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,ProjMatrix.data());

  glUniform3f(color_location,1,1,1);

  // Set up us the vertex data:
  glGenBuffers(2,buffers);

  {
    const float geometry[] = {
      -10.0, -10.0, 0.0,
      10.0, -10.0, 0.0,
      10.0, 10.0, 0.0,

      10.0, 10.0, 0.0,
      -10.0, 10.0, 0.0,
      -10.0, -10.0, 0.0
    };

    glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);

    glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 3 * 6,geometry,GL_STATIC_DRAW);
    glEnableVertexAttribArray(vertex_location);
    glVertexAttribPointer(vertex_location,3,GL_FLOAT,GL_FALSE,0,0);

    glBindBuffer(GL_ARRAY_BUFFER,0);
  }

  glGenQueries(2,queries);
}

extern "C"
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

  float xyz[3] = {
    compute_sine_wave(xyz_waves,rsxgltest_elapsed_time),
    compute_sine_wave(xyz_waves + 1,rsxgltest_elapsed_time),
    compute_sine_wave(xyz_waves + 2,rsxgltest_elapsed_time)
  };

  {
    glUniform3f(color_location,1,1,1);

    Eigen::Affine3f transmat = 
      Eigen::Affine3f::Identity() * 
      Eigen::Translation3f(xyz[0] * 20 - 10.0,0,0);
    
    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * transmat);
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    glDrawArrays(GL_TRIANGLES,0,6);
  }

  {
    glBeginQuery(GL_ANY_SAMPLES_PASSED,queries[0]);

    glUniform3f(color_location,1,0,0);

    Eigen::Affine3f transmat = 
      Eigen::Affine3f::Identity() * 
      Eigen::Translation3f(0,0,5.0);
    
    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * transmat);
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    glDrawArrays(GL_TRIANGLES,0,6);

    glEndQuery(GL_ANY_SAMPLES_PASSED);
  }

  {
#if 0
    glBeginQuery(GL_TIME_ELAPSED,queries[1]);
#endif

    glBeginConditionalRender(queries[0],GL_QUERY_NO_WAIT);

    glUniform3f(color_location,0,0,1);

    Eigen::Affine3f transmat = 
      Eigen::Affine3f::Identity() * 
      Eigen::Translation3f(0,-15,-5.0);
    
    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * transmat);
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    glDrawArrays(GL_TRIANGLES,0,6);

    glEndConditionalRender();

#if 0
    glEndQuery(GL_TIME_ELAPSED);

    GLuint64 elapsed_time = 0;
    glGetQueryObjectui64v(queries[1],GL_QUERY_RESULT,&elapsed_time);
    tcp_printf("elapsed time: %lu\n",(unsigned long)elapsed_time);
#endif
  }

  {
    glUniform3f(color_location,1,0,1);

    Eigen::Affine3f transmat = 
      Eigen::Affine3f::Identity() * 
      Eigen::Translation3f(0,15,5.0);
    
    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * transmat);
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    glDrawArrays(GL_TRIANGLES,0,6);
  }

  return 1;
}

extern "C"
void
rsxgltest_exit()
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  glDeleteShader(shaders[0]);
  glDeleteProgram(program);
  glDeleteShader(shaders[1]);
}
