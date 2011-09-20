/*
 * rsxgltest - textures
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#include <GL3/gl3ext.h>

#include "rsxgltest.h"
#include "sine_wave.h"

#include <stddef.h>
#include "textures_vpo.h"
#include "textures_fpo.h"

#include "texture.h"
#include "dice_bin.h"

char * rsxgltest_name = "uniforms";

extern void tcp_printf(const char * fmt,...);
extern void report_glerror(const char *);
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

Image dice;

GLuint buffer = 0;
GLuint shaders[2] = { 0,0 };
GLuint program = 0;
GLuint texture = 0;

GLint ProjMatrix_location = -1, TransMatrix_location = -1, c_location = -1, texture_location = -1;

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
  glShaderBinary(1,shaders,0,textures_vpo,textures_vpo_size);
  glShaderBinary(1,shaders + 1,0,textures_fpo,textures_fpo_size);

  // Link the program for real:
  glLinkProgram(program);
  glValidateProgram(program);

  GLint
    vertex_location = glGetAttribLocation(program,"vertex"),
    texcoord_location = glGetAttribLocation(program,"texcoord");
  tcp_printf("vertex_location: %i\n",vertex_location);
  tcp_printf("texcoord_location: %i\n",texcoord_location);

  ProjMatrix_location = glGetUniformLocation(program,"ProjMatrix");
  TransMatrix_location = glGetUniformLocation(program,"TransMatrix");
  c_location = glGetUniformLocation(program,"c");
  texture_location = glGetUniformLocation(program,"texture");
  tcp_printf("ProjMatrix_location: %i TransMatrix_location: %i c_location: %i texture_location: %i\n",
	     ProjMatrix_location,TransMatrix_location,c_location,texture_location);

  // Report on attributes:
  {
    GLint num_attribs = 0, attrib_name_length = 0;
    glGetProgramiv(program,GL_ACTIVE_ATTRIBUTES,&num_attribs);
    glGetProgramiv(program,GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,&attrib_name_length);
    tcp_printf("%u attribs, name max length: %u\n",num_attribs,attrib_name_length);
    char szName[attrib_name_length + 1];

    for(size_t i = 0;i < num_attribs;++i) {
      GLint size = 0;
      GLenum type = 0;
      GLint location = 0;
      glGetActiveAttrib(program,i,attrib_name_length + 1,0,&size,&type,szName);
      location = glGetAttribLocation(program,szName);
      tcp_printf("\t%u: %s %u %u %u\n",i,szName,(unsigned int)location,(unsigned int)size,(unsigned int)type);
    }
  }

  // Report on uniforms:
  {
    GLint num_uniforms = 0, uniform_name_length = 0;
    glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&num_uniforms);
    glGetProgramiv(program,GL_ACTIVE_UNIFORM_MAX_LENGTH,&uniform_name_length);
    tcp_printf("%u uniforms, name max length: %u\n",num_uniforms,uniform_name_length);
    char szName[uniform_name_length + 1];

    for(size_t i = 0;i < num_uniforms;++i) {
      GLint size = 0;
      GLenum type = 0;
      GLint location = 0;
      glGetActiveUniform(program,i,uniform_name_length + 1,0,&size,&type,szName);
      location = glGetUniformLocation(program,szName);
      tcp_printf("\t%u: %s %u %u %u\n",i,szName,(unsigned int)location,(unsigned int)size,(unsigned int)type);
    }
  }

  glUseProgram(program);

  const float identity[4][4] = { {1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1} };
  glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,(const GLfloat *)identity);
  glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,(const GLfloat *)identity);

  glUniform1i(texture_location,0);

  // Set up us the vertex data:
  float geometry[] = {
    0, 0, 0, 1,
    0,0,

    1.0, 0, 0, 1,
    1,0,

    0.0, 1.0, 0, 1,
    0,1,

    1.0, 0, 0, 1,
    1,0,

    1.0, 1.0, 0, 1,
    1,1,

    0.0, 1.0, 0, 1,
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

  // Textures:
  dice = loadPng(dice_bin);
  tcp_printf("dice size: %u %u\n",dice.width,dice.height);
  GLuint dice_buffer = 0;
  glGenBuffers(1,&dice_buffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,dice_buffer);
  glBufferData(GL_PIXEL_UNPACK_BUFFER,4 * dice.width * dice.height,dice.data,GL_STATIC_DRAW);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);

  uint8_t checkerboard_bgra[] = {
    255,0,0,255, 0,255,0,255,
    0,0,255,255, 255,255,255,255
  };

  uint8_t test_bgra[] = {
    192,128,64,255, 192,128,64,255,
    192,128,64,255, 192,128,64,255
  };

  glActiveTexture(GL_TEXTURE0);

  glGenTextures(1,&texture);
  glBindTexture(GL_TEXTURE_2D,texture);
  tcp_printf("texture name: %u\n",texture);

  //glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_BGRA,GL_UNSIGNED_BYTE,checkerboard_bgra);
  //glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_BGRA,GL_UNSIGNED_BYTE,0);
  //glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,dice.width,dice.height,0,GL_BGRA,GL_UNSIGNED_BYTE,dice.data);
  //glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,dice.width,dice.height,0,GL_BGRA,GL_UNSIGNED_BYTE,0);

  glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA,dice.width,dice.height);

  //glTexSubImage2D(GL_TEXTURE_2D,0,0,0,dice.width,dice.height,GL_BGRA,GL_UNSIGNED_BYTE,dice.data);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,dice_buffer);
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,dice.width,dice.height,GL_BGRA,GL_UNSIGNED_BYTE,0);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);

  report_glerror("glTexImage2D");

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
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
  //glClearColor(0,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUniform4f(c_location,1.0 - rgb[0],1.0 - rgb[1],1.0 - rgb[2],1);

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
