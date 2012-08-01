/*
 * rsxgltest - feedback1
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "rsxgltest.h"
#include "math3d.h"
#include "sine_wave.h"

#include <stddef.h>
#include "points_vert.h"
#include "feedback1_frag.h"

#include <io/pad.h>

#include <math.h>
#include <Eigen/Geometry>

const char * rsxgltest_name = "feedback1";

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

GLint ProjMatrix_location = -1, TransMatrix_location = -1;

const GLuint npoints = 100;
const GLuint batchsize = 256;
const GLuint nbatch = (npoints / batchsize) + ((npoints % batchsize) ? 1 : 0);

GLint batchfirst[nbatch];
GLsizei batchcount[nbatch];
GLvoid const * batchoffsets[nbatch];

GLuint * client_elements = 0;
GLvoid const * client_batchindices[nbatch];
GLuint elements = 0;

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

  const GLchar * shader_srcs[] = { (const GLchar *)points_vert, (const GLchar *)feedback1_frag };
  GLint shader_srcs_lengths[] = { points_vert_len, feedback1_frag_len };
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

  const char * varyings[] = {
    "gl_Position"
  };
  glTransformFeedbackVaryings(program,1,varyings,GL_SEPARATE_ATTRIBS);

  // Link the program for real:
  glLinkProgram(program);
  glValidateProgram(program);
  
  summarize_program("draw",program);

  GLint 
    vertex_location = glGetAttribLocation(program,"position");

  ProjMatrix_location = glGetUniformLocation(program,"ProjMatrix");
  TransMatrix_location = glGetUniformLocation(program,"TransMatrix");

  tcp_printf("vertex_location: %i\n",vertex_location);
  tcp_printf("ProjMatrix_location: %i TransMatrix_location: %i\n",
	     ProjMatrix_location,TransMatrix_location);

  glUseProgram(program);

  glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,ProjMatrix.data());

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

  glUnmapBuffer(GL_ARRAY_BUFFER);

  client_elements = (GLuint *)malloc(sizeof(GLuint) * npoints);
  for(size_t i = 0;i < npoints;++i) {
    client_elements[i] = i;
  }

  GLint first = 0;
  for(size_t i = 0;i < nbatch;++i) {
    batchfirst[i] = first;
    batchcount[i] = (i == (nbatch - 1)) ? (npoints % batchsize) : (batchsize);
    batchoffsets[i] = (GLvoid const*)((uint64_t)first * sizeof(GLuint));
    client_batchindices[i] = client_elements + first;
    first += batchsize;
  }

  glGenBuffers(1,&elements);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,elements);

  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(GLuint) * npoints,0,GL_STATIC_DRAW);
  
  GLuint * pelements = (GLuint *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER,GL_WRITE_ONLY);

  for(size_t i = 0;i < npoints;++i) {
    pelements[i] = i;
  }

  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);

  glEnableVertexAttribArray(vertex_location);
  glVertexAttribPointer(vertex_location,3,GL_FLOAT,GL_FALSE,sizeof(float) * 3 * 2,0);
  glPointSize(1);

  glBindBuffer(GL_ARRAY_BUFFER,0);

  glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffers[1]);
  
  const size_t nfeedback = sizeof(float) * 4 * npoints;
  GLfloat * pfeedback = (GLfloat *)malloc(nfeedback);
  GLfloat * ppfeedback = pfeedback;

  for(size_t i = 0;i < npoints;++i,ppfeedback += 4) {
    ppfeedback[0] = 3.0;
    ppfeedback[1] = 5.0;
    ppfeedback[2] = 7.0;
    ppfeedback[3] = (float)i;
  }

  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,nfeedback,pfeedback,GL_STATIC_DRAW);
  free(pfeedback);

  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffers[1],0,nfeedback);
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

  glPointSize(4.0);

  {
    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * Eigen::Translation3f(0,0,0) * rotmat);
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    //int which = frame % 6;
    int which = 0;
    tcp_printf("%s %i %i\n",__PRETTY_FUNCTION__,frame,which);

    if(which == 0) {
      report_glerror("Before transform feedback");

      glBeginTransformFeedback(GL_POINTS);
      report_glerror("glBeginTransformFeedback");

      glDrawArrays(GL_POINTS,0,npoints);
      report_glerror("glDrawArrays");

      glEndTransformFeedback();
      report_glerror("glEndTransformFeedback");

      // Dump the alleged feedback buffer:
      GLfloat * pfeedback = (GLfloat *)glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,GL_READ_ONLY);

      if(pfeedback) {
	for(size_t i = 0;i < npoints;++i,pfeedback += 4) {
	  tcp_printf("%04i: %.3f, %.3f, %.3f, %.3f\n",i,pfeedback[0],pfeedback[1],pfeedback[2],pfeedback[3]);
	}
      }

      glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    }
    else if(which == 1) {
      glMultiDrawArrays(GL_POINTS,batchfirst,batchcount,nbatch);
    }
    else if(which == 2) {
      glDrawElements(GL_POINTS,npoints,GL_UNSIGNED_INT,client_elements);
    }
    else if(which == 3) {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,elements);
      glDrawElements(GL_POINTS,npoints,GL_UNSIGNED_INT,0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    }
    else if(which == 4) {
      glMultiDrawElements(GL_POINTS,batchcount,GL_UNSIGNED_INT,client_batchindices,nbatch);
    }
    else if(which == 5) {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,elements);
      glMultiDrawElements(GL_POINTS,batchcount,GL_UNSIGNED_INT,batchoffsets,nbatch);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
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
