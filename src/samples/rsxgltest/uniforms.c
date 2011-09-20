/*
 * rsxgltest - uniforms
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "sine_wave.h"

#include <stddef.h>
typedef uint8_t u8;
typedef uint32_t u32;
#include "program_uniforms_vpo.h"
#include "program_uniforms_fpo.h"

char * rsxgltest_name = "uniforms";

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

GLint ProjMatrix_location = -1, TransMatrix_location = -1, c_location = -1;

static void
report_shader_info(GLuint shader)
{
  GLint type = 0, delete_status = 0, compile_status = 0;

  if(glIsShader(shader)) {
    glGetShaderiv(shader,GL_SHADER_TYPE,&type);
    glGetShaderiv(shader,GL_DELETE_STATUS,&delete_status);
    glGetShaderiv(shader,GL_COMPILE_STATUS,&compile_status);
    
    tcp_printf("shader: %u type: %x compile_status: %i delete_status: %i\n",shader,type,compile_status,delete_status);

    GLint nInfo = 0;
    glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&nInfo);
    if(nInfo > 0) {
      tcp_printf("\tinfo length: %u\n",nInfo);
      char szInfo[nInfo + 1];
      glGetShaderInfoLog(shader,nInfo + 1,0,szInfo);
      tcp_printf("\tinfo: %s\n",szInfo);
    }

  }
  else {
    tcp_printf("%u is not a shader\n",shader);
  }
}

static void
report_program_info(GLuint program)
{
  if(glIsProgram(program)) {
    GLint delete_status = 0, link_status = 0, validate_status = 0;

    glGetProgramiv(program,GL_DELETE_STATUS,&delete_status);
    glGetProgramiv(program,GL_LINK_STATUS,&link_status);
    glGetProgramiv(program,GL_VALIDATE_STATUS,&validate_status);
    
    tcp_printf("program: %u link_status: %i validate_status: %i delete_status: %i\n",program,link_status,validate_status,delete_status);

    GLint num_attached = 0;
    glGetProgramiv(program,GL_ATTACHED_SHADERS,&num_attached);
    tcp_printf("\tattached shaders: %u\n",num_attached);
    if(num_attached > 0) {
      GLuint attached[2] = { 0,0 };
      glGetAttachedShaders(program,2,0,attached);
      tcp_printf("\t");
      for(size_t i = 0;i < 2;++i) {
	if(attached[i] > 0) {
	  tcp_printf("%u ",attached[i]);
	}
      }
      tcp_printf("\n");
    }

    GLint nInfo = 0;
    glGetProgramiv(program,GL_INFO_LOG_LENGTH,&nInfo);
    if(nInfo > 0) {
      tcp_printf("\tinfo length: %u\n",nInfo);
      char szInfo[nInfo + 1];
      glGetProgramInfoLog(program,nInfo + 1,0,szInfo);
      tcp_printf("\tinfo: %s\n",szInfo);
    }
  }
  else {
    tcp_printf("%u is not a program\n",program);
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
  glShaderBinary(1,shaders,0,program_uniforms_vpo,program_uniforms_vpo_size);
  glShaderBinary(1,shaders + 1,0,program_uniforms_fpo,program_uniforms_fpo_size);

  // Link the program for real:
  glLinkProgram(program);
  glValidateProgram(program);

  summarize_program("uniforms",program);

  GLint
    vertex_location = glGetAttribLocation(program,"inputvertex.vertex"),
    texcoord_location = glGetAttribLocation(program,"inputvertex.texcoord");
  tcp_printf("vertex_location: %i\n",vertex_location);
  tcp_printf("texcoord_location: %i\n",texcoord_location);

  ProjMatrix_location = glGetUniformLocation(program,"ProjMatrix");
  TransMatrix_location = glGetUniformLocation(program,"TransMatrix");
  c_location = glGetUniformLocation(program,"c");
  tcp_printf("ProjMatrix_location: %i TransMatrix_location: %i c_location: %i\n",
	     ProjMatrix_location,TransMatrix_location,c_location);

  glUseProgram(program);

  const float identity[4][4] = { {1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1} };
  glUniformMatrix4fv(ProjMatrix_location,1,GL_FALSE,(const GLfloat *)identity);
  glUniformMatrix4fv(TransMatrix_location,1,GL_FALSE,(const GLfloat *)identity);

  // Set up us the vertex data:
  float geometry[] = {
    0, 0, 0, 1,
    0.5, 0, 0, 1,
    0.0, 0.5, 0, 1,

    0.5, 0, 0, 1,
    0.5, 0.5, 0, 1,
    0.0, 0.5, 0, 1
  };

  glGenBuffers(1,&buffer);
  glBindBuffer(GL_ARRAY_BUFFER,buffer);
  glBufferData(GL_ARRAY_BUFFER,sizeof(float) * 4 * 6,geometry,GL_STATIC_DRAW);
  glEnableVertexAttribArray(vertex_location);
  glVertexAttribPointer(vertex_location,4,GL_FLOAT,GL_FALSE,0,0);
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
