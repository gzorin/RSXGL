/*
 * rsxgltest - cube
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "rsxgltest.h"
#include "math3d.h"
#include "sine_wave.h"

#include <stddef.h>
#include "cube_vpo.h"
#include "cube_fpo.h"

#include <io/pad.h>

#include <math.h>
#include <Eigen/Geometry>

const char * rsxgltest_name = "cube";

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
    1.0f / 4.0f
  },
  {
    0.5f,
    0.5f,
    1.5f / 4.0f
  },
  {
    0.5f,
    0.5f,
    2.5f / 4.0f
  }
};

GLuint buffers[2] = { 0,0 };
GLuint shaders[2] = { 0,0 };
GLuint program = 0;

GLint ProjMatrix_location = -1, TransMatrix_location = -1, vertex_location = -1, color_location = -1;

const float geometry[] = {
    // -X
    -0.5,-0.5,-0.5,
    1,0,0,

    -0.5,0.5,-0.5,
    1,0,0,

    -0.5,0.5,0.5,
    1,0,0,

    -0.5,-0.5,0.5,
    1,0,0,

    // +X
    0.5,-0.5,-0.5,
    1,0,0,

    0.5,-0.5,0.5,
    1,0,0,

    0.5,0.5,0.5,
    1,0,0,

    0.5,0.5,-0.5,
    1,0,0,

    // -Y
    -0.5,-0.5,-0.5,
    0,1,0,

    -0.5,-0.5,0.5,
    0,1,0,

    0.5,-0.5,0.5,
    0,1,0,

    0.5,-0.5,-0.5,
    0,1,0,

    // +Y
    -0.5,0.5,-0.5,
    0,1,0,

    0.5,0.5,-0.5,
    0,1,0,

    0.5,0.5,0.5,
    0,1,0,

    -0.5,0.5,0.5,
    0,1,0,

    // -Z
    -0.5,-0.5,-0.5,
    0,0,1,

    -0.5,0.5,-0.5,
    0,0,1,

    0.5,0.5,-0.5,
    0,0,1,

    0.5,-0.5,-0.5,
    0,0,1,

    // +Z
    -0.5,-0.5,0.5,
    0,0,1,

    -0.5,0.5,0.5,
    0,0,1,

    0.5,0.5,0.5,
    0,0,1,

    0.5,-0.5,0.5,
    0,0,1
  };

  const GLuint indices[] = {
    // -X
    0, 1, 2,
    2, 3, 0,

    // +X
    4, 5, 6,
    6, 7, 4,

    // -Y
    8, 9, 10,
    10, 11, 8,

    // +Y
    12, 13, 14,
    14, 15, 12,

    // -Z
    16, 17, 18,
    18, 19, 16,

    // +Z
    20, 21, 22,
    22, 23, 20
  };

GLuint * client_indices = 0;

#define DTOR(X) ((X)*0.01745329f)
#define RTOD(d) ((d)*57.295788f)

Eigen::Projective3f ProjMatrix(perspective(DTOR(54.3),1920.0 / 1080.0,0.1,1000.0));

Eigen::Affine3f ViewMatrixInv = 
  Eigen::Affine3f(Eigen::Affine3f::Identity() * 
		  (
		   Eigen::Translation3f(1.779,2.221,4.034) *
		   (
		    Eigen::AngleAxisf(DTOR(0),Eigen::Vector3f::UnitZ()) *
		    Eigen::AngleAxisf(DTOR(23.8),Eigen::Vector3f::UnitY()) *
		    Eigen::AngleAxisf(DTOR(-26.738),Eigen::Vector3f::UnitX())
		    )
		   )
		  ).inverse();

extern "C"
void
rsxgltest_pad(const padData * paddata)
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
  glShaderBinary(1,shaders,0,cube_vpo,cube_vpo_size);
  glShaderBinary(1,shaders + 1,0,cube_fpo,cube_fpo_size);

  // Link the program for real:
  glLinkProgram(program);
  glValidateProgram(program);
  
  summarize_program("draw",program);

  vertex_location = glGetAttribLocation(program,"inputvertex.vertex");
  color_location = glGetAttribLocation(program,"inputvertex.color");

  ProjMatrix_location = glGetUniformLocation(program,"ProjMatrix");
  TransMatrix_location = glGetUniformLocation(program,"TransMatrix");

  tcp_printf("vertex_location: %i\n",vertex_location);
  tcp_printf("color_location: %i\n",color_location);
  tcp_printf("ProjMatrix_location: %i TransMatrix_location: %i\n",
	     ProjMatrix_location,TransMatrix_location);

  glUseProgram(program);

  glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,ProjMatrix.data());

  // Set up us the vertex data:
  glGenBuffers(2,buffers);

  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);

  glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 6 * 4 * 6,geometry,GL_STATIC_DRAW);
  glEnableVertexAttribArray(vertex_location);
  glEnableVertexAttribArray(color_location);
  glVertexAttribPointer(vertex_location,3,GL_FLOAT,GL_FALSE,sizeof(float) * 6,0);
  glVertexAttribPointer(color_location,3,GL_FLOAT,GL_FALSE,sizeof(float) * 6,(const GLvoid *)(sizeof(float) * 3));

  //glBindBuffer(GL_ARRAY_BUFFER,0);

  //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[1]);
  //glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(GLuint) * 6 * 2 * 3,indices,GL_STATIC_DRAW);

  client_indices = (GLuint *)malloc(sizeof(GLuint) * 6 * 2 * 3);
  memcpy(client_indices,indices,sizeof(GLuint) * 6 * 2 * 3);
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

  Eigen::Affine3f rotmat = 
    Eigen::Affine3f::Identity() * 
    Eigen::AngleAxisf(DTOR(xyz[2]) * 360.0f,Eigen::Vector3f::UnitZ()) *
    Eigen::AngleAxisf(DTOR(xyz[1]) * 360.0f,Eigen::Vector3f::UnitY()) *
    Eigen::AngleAxisf(DTOR(xyz[0]) * 360.0f,Eigen::Vector3f::UnitX());

  // Test out orphaning:
  glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 6 * 4 * 6,geometry,GL_STATIC_DRAW);

  {
    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * Eigen::Translation3f(-5,0,0) * rotmat);
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    static const float red[4] = { 1,0,0,1 };
    //glVertexAttrib4fv(color_location,red);
    //glDisableVertexAttribArray(color_location);
    //glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0);
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,client_indices);
    glFinish();
  }

  {
    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * Eigen::Translation3f(5,0,0) * rotmat);
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    static const float blue[4] = { 0,0,1,1 };
    //glVertexAttrib4fv(color_location,blue);
    //glEnableVertexAttribArray(color_location);
    //glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0);
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,client_indices);
    glFinish();
  }

  return 1;
}

extern "C"
void
rsxgltest_exit()
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  glBindBuffer(GL_ARRAY_BUFFER,0);

  glVertexAttribPointer(vertex_location,3,GL_FLOAT,GL_FALSE,0,0);
  glVertexAttribPointer(color_location,3,GL_FLOAT,GL_FALSE,0,0);

  glDeleteBuffers(2,buffers);

  glDeleteShader(shaders[0]);
  glDeleteProgram(program);
  glDeleteShader(shaders[1]);
}
