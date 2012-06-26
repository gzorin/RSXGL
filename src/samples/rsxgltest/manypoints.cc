/*
 * rsxgltest - manypoints
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "rsxgltest.h"
#include "math3d.h"
#include "sine_wave.h"

#include <stddef.h>
#include "points_vert.h"
#include "points_frag.h"

#include <io/pad.h>

#include <math.h>
#include <Eigen/Geometry>

#include "texture.h"
#include "face_png.h"

const char * rsxgltest_name = "manypoints";

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

Image image;
GLuint texture = 0;

GLint ProjMatrix_location = -1, TransMatrix_location = -1, color_location = -1, texture_location = -1;

const GLuint npoints = 1000;
const GLuint batchsize = 256;
const GLuint nbatch = (npoints / batchsize) + ((npoints % batchsize) ? 1 : 0);

GLint batchfirst[nbatch];
GLsizei batchcount[nbatch];

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
  //glShaderBinary(1,shaders,0,points_vpo,points_vpo_size);
  //glShaderBinary(1,shaders + 1,0,points_fpo,points_fpo_size);

  // Supply shader SOURCES!
  char szInfo[2048];

  const GLchar * shader_srcs[] = { (const GLchar *)points_vert, (const GLchar *)points_frag };
  GLint shader_srcs_lengths[] = { points_vert_len, points_frag_len };
  GLint compiled = 0;

  glShaderSource(shaders[0],1,shader_srcs,shader_srcs_lengths);
  glCompileShader(shaders[0]);

  glGetShaderiv(shaders[0],GL_COMPILE_STATUS,&compiled);
  tcp_printf("shader compile status: %i\n",compiled);

  glGetShaderInfoLog(shaders[0],2048,0,szInfo);
  tcp_printf("%s\n",szInfo);

  glShaderSource(shaders[1],1,shader_srcs + 1,shader_srcs_lengths + 1);
  glCompileShader(shaders[1]);

  glGetShaderiv(shaders[1],GL_COMPILE_STATUS,&compiled);
  tcp_printf("shader compile status: %i\n",compiled);

  glGetShaderInfoLog(shaders[1],2048,0,szInfo);
  tcp_printf("%s\n",szInfo);

  // Link the program for real:
  glLinkProgram(program);
  glValidateProgram(program);
  
  summarize_program("draw",program);

  GLint 
    vertex_location = glGetAttribLocation(program,"position");

  color_location = glGetAttribLocation(program,"color");

  ProjMatrix_location = glGetUniformLocation(program,"ProjMatrix");
  TransMatrix_location = glGetUniformLocation(program,"TransMatrix");
  texture_location = glGetUniformLocation(program,"texture");

  tcp_printf("vertex_location: %i\n",vertex_location);
  tcp_printf("color_location: %i\n",color_location);
  tcp_printf("ProjMatrix_location: %i TransMatrix_location: %i\n",
	     ProjMatrix_location,TransMatrix_location);
  tcp_printf("texture_location: %i\n",texture_location);

  glUseProgram(program);

  glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,ProjMatrix.data());

  glUniform1i(texture_location,0);

  // Set up us the vertex data:
  glGenBuffers(2,buffers);

  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);

  glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 3 * 2 * npoints,0,GL_STATIC_DRAW);

  float * geometry = (float *)glMapBuffer(GL_ARRAY_BUFFER,GL_WRITE_ONLY);

  for(size_t i = 0;i < npoints;++i,geometry += 6) {
    geometry[0] = drand48() * 5.0 - 2.5;
    geometry[1] = drand48() * 5.0 - 2.5;
    geometry[2] = drand48() * 5.0 - 2.5;
    geometry[3] = 1.0;
    geometry[4] = 1.0;
    geometry[5] = 1.0;
  }

  GLint first = 0;
  for(size_t i = 0;i < nbatch;++i) {
    batchfirst[i] = first;
    batchcount[i] = (i == (nbatch - 1)) ? (npoints % batchsize) : (batchsize);
    first += batchsize;
  }

  glUnmapBuffer(GL_ARRAY_BUFFER);

  glEnableVertexAttribArray(vertex_location);
  //glEnableVertexAttribArray(color_location);
  glVertexAttribPointer(vertex_location,3,GL_FLOAT,GL_FALSE,sizeof(float) * 3 * 2,0);
  //glVertexAttribPointer(color_location,3,GL_FLOAT,GL_FALSE,sizeof(float) * 3 * 2,(const GLvoid *)(sizeof(float) * 3));
  glPointSize(64);

  glBindBuffer(GL_ARRAY_BUFFER,0);

  // Texture map:
  image = loadPng(face_png);
  tcp_printf("image size: %u %u\n",image.width,image.height);

  glGenTextures(1,&texture);
  glBindTexture(GL_TEXTURE_2D,texture);

  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image.width,image.height,0,GL_BGRA,GL_UNSIGNED_BYTE,image.data);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
}

extern "C"
int
rsxgltest_draw()
{
  static int frame = 0;

  float rgb[3] = {
    compute_sine_wave(rgb_waves,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 1,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 2,rsxgltest_elapsed_time)
  };

  glClearColor(0,0,0,1.0);
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

  {
    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * Eigen::Translation3f(0,0,0) * rotmat);
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    if(frame % 2) {
      glDrawArrays(GL_POINTS,0,npoints);
    }
    else {
      glMultiDrawArrays(GL_POINTS,batchfirst,batchcount,nbatch);
    }
  }

  ++frame;

  return 1;
}

extern "C"
void
rsxgltest_exit()
{
  glDeleteShader(shaders[0]);
  glDeleteProgram(program);
  glDeleteShader(shaders[1]);
}
