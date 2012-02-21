/*
 * rsxgltest - textured cube
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "rsxgltest.h"
#include "math3d.h"
#include "sine_wave.h"

#include <stddef.h>
#include "texcube_vert.h"
#include "texcube_frag.h"

#include <io/pad.h>

#include <math.h>
#include <Eigen/Geometry>

#include "texture.h"
#include "nagel_bin.h"
#include "face_png.h"
#include "gradient_png.h"

const char * rsxgltest_name = "texcube";

struct sine_wave_t rgb_waves[3] = {
  { 0.5f,
    0.5f,
    1.0f
  },
  { 0.5f,
    0.5f,
    1.5f
  },
  { 0.5f,
    0.5f,
    2.5f
  }
};

struct sine_wave_t xyz_waves[3] = {
  { 0.5f,
    0.5f,
    1.0f / 4.0f
  },
  { 0.5f,
    0.5f,
    1.5f / 4.0f
  },
  { 0.5f,
    0.5f,
    2.5f / 4.0f
  }
};

GLuint vao = 0;
GLuint buffers[2] = { 0,0 };
GLuint shaders[2] = { 0,0 };

Image nagel_image, face_image, gradient_image;
GLuint textures[3] = { 0,0,0 };
GLuint program = 0;

GLint ProjMatrix_location = -1, TransMatrix_location = -1, vertex_location = -1, tc_location = -1, image_location = -1, gradient_location = -1;

const float geometry[] = {
    // -X
    -0.5,-0.5,-0.5,
    0,0,

    -0.5,0.5,-0.5,
    1,0,

    -0.5,0.5,0.5,
    1,1,

    -0.5,-0.5,0.5,
    0,1,

    // +X
    0.5,-0.5,-0.5,
    0,0,

    0.5,-0.5,0.5,
    1,0,

    0.5,0.5,0.5,
    1,1,

    0.5,0.5,-0.5,
    0,1,

    // -Y
    -0.5,-0.5,-0.5,
    0,0,

    -0.5,-0.5,0.5,
    1,0,

    0.5,-0.5,0.5,
    1,1,

    0.5,-0.5,-0.5,
    0,1,

    // +Y
    -0.5,0.5,-0.5,
    0,0,

    0.5,0.5,-0.5,
    1,0,

    0.5,0.5,0.5,
    1,1,

    -0.5,0.5,0.5,
    0,1,

    // -Z
    -0.5,-0.5,-0.5,
    0,0,

    -0.5,0.5,-0.5,
    1,0,

    0.5,0.5,-0.5,
    1,1,

    0.5,-0.5,-0.5,
    0,1,

    // +Z
    -0.5,-0.5,0.5,
    0,0,

    -0.5,0.5,0.5,
    1,0,

    0.5,0.5,0.5,
    1,1,

    0.5,-0.5,0.5,
    0,1
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

#if 0
  // Supply shader binaries:
  glShaderBinary(1,shaders,0,texcube_vpo,texcube_vpo_size);
  glShaderBinary(1,shaders + 1,0,texcube_fpo,texcube_fpo_size);
#endif

  // Supply shader SOURCES!
  const GLchar * shader_srcs[] = { (const GLchar *)texcube_vert, (const GLchar *)texcube_frag };
  GLint shader_srcs_lengths[] = { texcube_vert_len, texcube_frag_len };
  GLint compiled = 0;

  glShaderSource(shaders[0],1,shader_srcs,shader_srcs_lengths);
  glCompileShader(shaders[0]);

  glGetShaderiv(shaders[0],GL_COMPILE_STATUS,&compiled);
  tcp_printf("shader compile status: %i\n",compiled);

  glShaderSource(shaders[1],1,shader_srcs + 1,shader_srcs_lengths + 1);
  glCompileShader(shaders[1]);

  glGetShaderiv(shaders[1],GL_COMPILE_STATUS,&compiled);
  tcp_printf("shader compile status: %i\n",compiled);

  // Link the program for real:
  glLinkProgram(program);

#if 0
  glValidateProgram(program);
  
  summarize_program("draw",program);
#endif

#if 0
  vertex_location = glGetAttribLocation(program,"vertex");
  tc_location = glGetAttribLocation(program,"uv");

  ProjMatrix_location = glGetUniformLocation(program,"ProjMatrix");
  TransMatrix_location = glGetUniformLocation(program,"TransMatrix");
  image_location = glGetUniformLocation(program,"image");
  gradient_location = glGetUniformLocation(program,"gradient");

  tcp_printf("vertex_location: %i\n",vertex_location);
  tcp_printf("tc_location: %i\n",tc_location);
  tcp_printf("ProjMatrix_location: %i TransMatrix_location: %i\n",
	     ProjMatrix_location,TransMatrix_location);
  tcp_printf("image_location: %i gradient_location: %i\n",image_location,gradient_location);

  glUseProgram(program);

  glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,ProjMatrix.data());

  glUniform1i(image_location,0);
  glUniform1i(gradient_location,2);

  // Set up us the vertex data:
  glGenBuffers(2,buffers);

  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);

  glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 6 * 4 * 5,geometry,GL_STATIC_DRAW);
  glEnableVertexAttribArray(vertex_location);
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(vertex_location,3,GL_FLOAT,GL_FALSE,sizeof(float) * 5,0);
  glVertexAttribPointer(tc_location,2,GL_FLOAT,GL_FALSE,sizeof(float) * 5,(const GLvoid *)(sizeof(float) * 3));

  //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[1]);
  //glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(GLuint) * 6 * 2 * 3,indices,GL_STATIC_DRAW);

  client_indices = (GLuint *)malloc(sizeof(GLuint) * 6 * 2 * 3);
  memcpy(client_indices,indices,sizeof(GLuint) * 6 * 2 * 3);

  // Texture map:
  nagel_image = loadPng(nagel_bin);
  face_image = loadPng(face_png);
  gradient_image = loadPng(gradient_png);

  glGenTextures(3,textures);

  //
  glActiveTexture(GL_TEXTURE0);

  glBindTexture(GL_TEXTURE_2D,textures[0]);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,nagel_image.width,nagel_image.height,0,GL_BGRA,GL_UNSIGNED_BYTE,nagel_image.data);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

  //
  glActiveTexture(GL_TEXTURE1);

  glBindTexture(GL_TEXTURE_2D,textures[1]);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,face_image.width,face_image.height,0,GL_BGRA,GL_UNSIGNED_BYTE,face_image.data);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

  //
  glActiveTexture(GL_TEXTURE2);

  glBindTexture(GL_TEXTURE_2D,textures[2]);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,gradient_image.width,gradient_image.height,0,GL_BGRA,GL_UNSIGNED_BYTE,gradient_image.data);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
#endif
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

#if 0
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
    //glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D,textures[0]);
    glUniform1i(image_location,0);

    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * Eigen::Translation3f(-5,0,0) * rotmat * Eigen::UniformScaling< float >(3.0));
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,client_indices);
  }

  {
    //glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D,textures[1]);
    glUniform1i(image_location,1);

    Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * Eigen::Translation3f(5,0,0) * rotmat * Eigen::UniformScaling< float >(3.0));
    glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,modelview.data());

    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,client_indices);
  }
#endif

  return 1;
}

extern "C"
void
rsxgltest_exit()
{
#if 0
  glBindBuffer(GL_ARRAY_BUFFER,0);

  glVertexAttribPointer(vertex_location,3,GL_FLOAT,GL_FALSE,0,0);
  glVertexAttribPointer(tc_location,2,GL_FLOAT,GL_FALSE,0,0);

  glDeleteBuffers(2,buffers);

  glDeleteShader(shaders[0]);
  glDeleteProgram(program);
  glDeleteShader(shaders[1]);
#endif
}
