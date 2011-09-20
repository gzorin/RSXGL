/*
 * rsxgltest - program
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include "sine_wave.h"

#include <stddef.h>
typedef uint8_t u8;
typedef uint32_t u32;
#include "program_vpo.h"
#include "program_fpo.h"

char * rsxgltest_name = "program";

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

GLuint shaders[2] = { 0,0 };
GLuint program = 0;

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
  //
  tcp_printf("%s\n",__PRETTY_FUNCTION__);

  //
  report_shader_info(0);

  shaders[0] = glCreateShader(42);
  report_glerror("bad create shader 42");

  glDeleteShader(0);
  report_glerror("bad delete shader 0");

  glDeleteShader(42);
  report_glerror("bad delete shader 42");

  report_shader_info(42);

  shaders[0] = glCreateShader(GL_VERTEX_SHADER);
  report_glerror("create vertex shader");
  report_shader_info(shaders[0]);

  shaders[1] = glCreateShader(GL_FRAGMENT_SHADER);
  report_glerror("create fragment shader");
  report_shader_info(shaders[0]);
  report_shader_info(shaders[1]);

  glDeleteShader(shaders[0]);
  report_glerror("delete vertex shader");
  report_shader_info(shaders[0]);

  glDeleteShader(shaders[1]);
  report_glerror("delete fragment shader");
  report_shader_info(shaders[1]);

  //
  report_program_info(0);
  report_program_info(42);

  program = glCreateProgram();
  report_glerror("create program");
  report_program_info(program);

  glDeleteProgram(program);
  report_glerror("delete program");
  report_program_info(program);

  // try attaching shaders:
  shaders[0] = glCreateShader(GL_VERTEX_SHADER);
  report_glerror("create vertex shader");
  report_shader_info(shaders[0]);

  shaders[1] = glCreateShader(GL_FRAGMENT_SHADER);
  report_glerror("create fragment shader");
  report_shader_info(shaders[1]);

  program = glCreateProgram();
  report_glerror("create program");
  report_program_info(program);

  glAttachShader(program,0);
  report_glerror("bad attach 0");
  report_program_info(program);

  glAttachShader(program,42);
  report_glerror("bad attach 42");
  report_program_info(program);

  // Try linking the program, without any shaders attached:
  glLinkProgram(program);
  report_program_info(program);

  glAttachShader(program,shaders[0]);
  report_glerror("attach shader 0");
  report_program_info(program);

  glAttachShader(program,shaders[1]);
  report_glerror("attach shader 1");
  report_program_info(program);

  // Try linking the program, with empty shaders attached:
  glLinkProgram(program);
  report_program_info(program);

  // Supply shader binaries:
  glShaderBinary(1,shaders,0,program_vpo,program_vpo_size);
  report_glerror("shader binary 0");
  glShaderBinary(1,shaders + 1,0,program_fpo,program_fpo_size);
  report_glerror("shader binary 1");

  // Bind vertex to a different index:
  // (Actually, don't do this now):
  //glBindAttribLocation(program,3,"inputvertex.vertex");

  // Link the program for real:
  glLinkProgram(program);
  report_program_info(program);

  glValidateProgram(program);
  report_program_info(program);

  // Report on attributes:
  {
    GLint num_attribs = 0, attrib_name_length = 0;
    glGetProgramiv(program,GL_ACTIVE_ATTRIBUTES,&num_attribs);
    glGetProgramiv(program,GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,&attrib_name_length);
    tcp_printf("%u attribs, name max length: %u\n",num_attribs,attrib_name_length);
    char szName[attrib_name_length + 1];
  }

  glDeleteShader(shaders[0]);
  glDeleteProgram(program);
  glDeleteShader(shaders[1]);
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
  return 1;
}

void
rsxgltest_exit()
{
}
