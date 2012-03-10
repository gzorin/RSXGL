/*
 * rsxgltest - framebuffer object demo. A cube within a cube, maaaaaaan!
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#include <GL3/gl3ext.h>

#include "rsxgltest.h"
#include "math3d.h"
#include "sine_wave.h"

#include <stddef.h>
#include "fbo_inner_vert.h"
#include "fbo_inner_frag.h"
#include "fbo_outer_vert.h"
#include "fbo_outer_frag.h"

#include <io/pad.h>

#include <math.h>
#include <Eigen/Geometry>

#include "texture.h"
#include "nagel_bin.h"

const char * rsxgltest_name = "fbomrt";

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

Image image;
GLuint textures[3] = { 0,0,0 };
GLuint program = 0;

GLuint fbo = 0, rbo[2] = { 0,0 };

const float geometry[] = {
    // -X
    -0.5,-0.5,-0.5,
    -1.0,0.0,0.0,
    0,0,

    -0.5,0.5,-0.5,
    -1.0,0.0,0.0,
    1,0,

    -0.5,0.5,0.5,
    -1.0,0.0,0.0,
    1,1,

    -0.5,-0.5,0.5,
    -1.0,0.0,0.0,
    0,1,

    // +X
    0.5,-0.5,-0.5,
    1.0,0.0,0.0,
    0,0,

    0.5,-0.5,0.5,
    1.0,0.0,0.0,
    1,0,

    0.5,0.5,0.5,
    1.0,0.0,0.0,
    1,1,

    0.5,0.5,-0.5,
    1.0,0.0,0.0,
    0,1,

    // -Y
    -0.5,-0.5,-0.5,
    0.0,-1.0,0.0,
    0,0,

    -0.5,-0.5,0.5,
    0.0,-1.0,0.0,
    1,0,

    0.5,-0.5,0.5,
    0.0,-1.0,0.0,
    1,1,

    0.5,-0.5,-0.5,
    0.0,-1.0,0.0,
    0,1,

    // +Y
    -0.5,0.5,-0.5,
    0.0,1.0,0.0,
    0,0,

    0.5,0.5,-0.5,
    0.0,1.0,0.0,
    1,0,

    0.5,0.5,0.5,
    0.0,1.0,0.0,
    1,1,

    -0.5,0.5,0.5,
    0.0,1.0,0.0,
    0,1,

    // -Z
    -0.5,-0.5,-0.5,
    0.0,0.0,-1.0,
    0,0,

    -0.5,0.5,-0.5,
    0.0,0.0,-1.0,
    1,0,

    0.5,0.5,-0.5,
    0.0,0.0,-1.0,
    1,1,

    0.5,-0.5,-0.5,
    0.0,0.0,-1.0,
    0,1,

    // +Z
    -0.5,-0.5,0.5,
    0.0,0.0,1.0,
    0,0,

    -0.5,0.5,0.5,
    0.0,0.0,1.0,
    1,0,

    0.5,0.5,0.5,
    0.0,0.0,1.0,
    1,1,

    0.5,-0.5,0.5,
    0.0,0.0,1.0,
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

// Projection matrix for the outer cube:
Eigen::Projective3f ProjMatrix(perspective(DTOR(54.3),1920.0 / 1080.0,0.1,1000.0));

// Projection matrix for the inner cube - the image that gets rendered to the FBO:
Eigen::Projective3f ProjMatrix2(perspective(DTOR(35.3),1920.0 / 1080.0,0.1,1000.0));

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

const GLfloat light[4] = { 10.0f, 10.0f, 10.0f, 1.0f };

struct program {
  GLuint object;
  int vertex, normal, uv, ProjMatrix, TransMatrix, NormalMatrix, light, tc, texture;

  program() 
    : object(0),
      vertex(-1),
      normal(-1),
      uv(-1),
      ProjMatrix(-1),
      TransMatrix(-1),
      NormalMatrix(-1),
      light(-1),
      tc(-1),
      texture(-1) {
  }
  
  void init(const unsigned char * vp,const size_t vp_len,
	    const unsigned char * fp,const size_t fp_len) {
    GLuint shaders[2] = { 0,0 };

    const GLchar * shader_srcs[2] = { (const GLchar *)vp, (const GLchar *)fp };
    GLint shader_srcs_lengths[2] = { vp_len, fp_len };
    int worked = 0;
    GLchar szInfo[1024];
    GLuint shader_types[2] = {
      GL_VERTEX_SHADER,
      GL_FRAGMENT_SHADER
    };
    
    for(int i = 0;i < 2;++i) {
      shaders[i] = glCreateShader(shader_types[i]);

      glShaderSource(shaders[i],1,shader_srcs + i,shader_srcs_lengths + i);
      glCompileShader(shaders[i]);

      glGetShaderInfoLog(shaders[i],1024,0,szInfo);
      tcp_printf("info log for shader %u: %s\n",shaders[i],szInfo);

      GLint compiled = 0;
      glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&compiled);

      if(!compiled) continue;

      ++worked;
    }

    if(worked == 2) {
      object = glCreateProgram();
      for(int i = 0;i < 2;++i) {
	glAttachShader(object,shaders[i]);
      }

      glBindAttribLocation(object,0,"vertex");
      glBindAttribLocation(object,1,"normal");
      glBindAttribLocation(object,2,"uv");

      glLinkProgram(object);
      glGetProgramInfoLog(object,1024,0,szInfo);
      tcp_printf("info log for program %u: %s\n",object,szInfo);

      GLint linked = 0;
      glGetProgramiv(object,GL_LINK_STATUS,&linked);

      if(linked) {
	glValidateProgram(object);

	vertex = glGetAttribLocation(object,"vertex");
	normal = glGetAttribLocation(object,"normal");
	tc = glGetAttribLocation(object,"uv");
	
	tcp_printf("attrib locations: %i %i %i\n",
		   vertex,
		   normal,
		   tc);
	
	ProjMatrix = glGetUniformLocation(object,"ProjMatrix");
	TransMatrix = glGetUniformLocation(object,"TransMatrix");
	NormalMatrix = glGetUniformLocation(object,"NormalMatrix");
	texture = glGetUniformLocation(object,"texture");
	light = glGetUniformLocation(object,"light");
	
	tcp_printf("vertex: %i\n",vertex);
	tcp_printf("normal: %i\n",normal);
	tcp_printf("tc: %i\n",tc);
	tcp_printf("ProjMatrix: %i TransMatrix: %i\n",
		   ProjMatrix,TransMatrix);
	tcp_printf("texture: %i\n",texture);
      }
      else {
	glDeleteProgram(object);
	object = 0;
      }
    }
  }
};

struct program inner_program, outer_program;

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

  // Setup programs:
  inner_program.init(fbo_inner_vert,fbo_inner_vert_len,
		     fbo_inner_frag,fbo_inner_frag_len);

  outer_program.init(fbo_outer_vert,fbo_outer_vert_len,
		     fbo_outer_frag,fbo_outer_frag_len);

  // Set up us the vertex data:
  glGenBuffers(2,buffers);

  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);

  glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 6 * 4 * 8,geometry,GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(float) * 8,0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(float) * 8,(const GLvoid *)(sizeof(float) * 3));

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(float) * 8,(const GLvoid *)(sizeof(float) * 6));

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[1]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(GLuint) * 6 * 2 * 3,indices,GL_STATIC_DRAW);

  client_indices = (GLuint *)malloc(sizeof(GLuint) * 6 * 2 * 3);
  memcpy(client_indices,indices,sizeof(GLuint) * 6 * 2 * 3);

  // Texture map:
  image = loadPng(nagel_bin);
  tcp_printf("image size: %u %u\n",image.width,image.height);

  glGenTextures(3,textures);

  tcp_printf("\ttextures: %u %u %u\n",textures[0],textures[1],textures[2]);

  // image asset:
  glBindTexture(GL_TEXTURE_2D,textures[0]);

  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.data);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

  // rendering surface:
  for(int i = 1;i < 3;++i) {
    glBindTexture(GL_TEXTURE_2D,textures[i]);

    //glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA,image.width,image.height);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
    
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  }

  // Framebuffer object:
  glGenRenderbuffers(2,rbo);
  report_glerror("glGenRenderbuffers");

  //glBindRenderbuffer(GL_RENDERBUFFER,rbo[0]);
  //glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA,image.width,image.height);

  glBindRenderbuffer(GL_RENDERBUFFER,rbo[1]);
  glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT,image.width,image.height);

  glBindRenderbuffer(GL_RENDERBUFFER,0);

  //
  glGenFramebuffers(1,&fbo);

  glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  //glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,rbo[0]);
  //glFramebufferTexture(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,texture,0);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[1],0);
  //glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,textures[2],0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,rbo[1]);

  glBindFramebuffer(GL_FRAMEBUFFER,0);

  glBindTexture(GL_TEXTURE_2D,0);
}

extern "C"
int
rsxgltest_draw()
{
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  float rgb[3] = {
    compute_sine_wave(rgb_waves,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 1,rsxgltest_elapsed_time),
    compute_sine_wave(rgb_waves + 2,rsxgltest_elapsed_time)
  };

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

  //Eigen::Affine3f modelview = ViewMatrixInv * (Eigen::Affine3f::Identity() * rotmat * Eigen::UniformScaling< float >(3.0));

  // Render to FBO:
  glBindFramebuffer(GL_FRAMEBUFFER,fbo);

  if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
    //
    glViewport(0,0,image.width,image.height);

    glUseProgram(inner_program.object);
    
    glUniform1i(inner_program.texture,0);
    glUniform4fv(inner_program.light,1,light);
    
    glUniformMatrix4fv(inner_program.ProjMatrix,1,GL_FALSE,ProjMatrix2.data());
    
    Transform3f modelview = ViewMatrixInv * (Transform3f::Identity() * rotmat * Eigen::UniformScaling< float >(3.0));
    glUniformMatrix4fv(inner_program.TransMatrix,1,GL_FALSE,modelview.data());
    
    Eigen::Affine3f normal = Eigen::Affine3f::Identity();
    normal.linear() = modelview.linear().inverse().transpose();
    glUniformMatrix4fv(inner_program.NormalMatrix,1,GL_FALSE,normal.data());
    
    glClearColor(1.0 - rgb[0],1.0 - rgb[1],1.0 - rgb[2],1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glBindTexture(GL_TEXTURE_2D,textures[0]);
    
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0 /*client_indices*/);
  }
  else {
    tcp_printf("fbo is incomplete\n");
  }

  // Render to main framebuffer:
  //
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  glViewport(0,0,rsxgltest_width,rsxgltest_height);
  
  glClearColor(rgb[0],rgb[1],rgb[2],1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  {
    glUseProgram(outer_program.object);
    
    glUniform1i(outer_program.texture,0);
    glUniform4fv(outer_program.light,1,light);
    
    glUniformMatrix4fv(outer_program.ProjMatrix,1,GL_FALSE,ProjMatrix.data());
  }

  {
    glBindTexture(GL_TEXTURE_2D,textures[1]);

    Transform3f modelview = ViewMatrixInv * (Transform3f::Identity() * Eigen::Translation3f(-5,0,0) * rotmat * Eigen::UniformScaling< float >(3.0));
    glUniformMatrix4fv(outer_program.TransMatrix,1,GL_FALSE,modelview.data());
    
    Eigen::Affine3f normal = Eigen::Affine3f::Identity();
    normal.linear() = modelview.linear().inverse().transpose();
    glUniformMatrix4fv(outer_program.NormalMatrix,1,GL_FALSE,normal.data());

    glUniformMatrix4fv(outer_program.ProjMatrix,1,GL_FALSE,ProjMatrix.data());
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0 /*client_indices*/);
  }

  {
    glBindTexture(GL_TEXTURE_2D,textures[1]);

    Transform3f modelview = ViewMatrixInv * (Transform3f::Identity() * Eigen::Translation3f(5,0,0) * rotmat * Eigen::UniformScaling< float >(3.0));
    glUniformMatrix4fv(outer_program.TransMatrix,1,GL_FALSE,modelview.data());
    
    Eigen::Affine3f normal = Eigen::Affine3f::Identity();
    normal.linear() = modelview.linear().inverse().transpose();
    glUniformMatrix4fv(outer_program.NormalMatrix,1,GL_FALSE,normal.data());

    glUniformMatrix4fv(outer_program.ProjMatrix,1,GL_FALSE,ProjMatrix.data());
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0 /*client_indices*/);
  }

  return 1;
}

extern "C"
void
rsxgltest_exit()
{
}
