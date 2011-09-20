/*
 * rsxgltest - clear
 */

#define GL3_PROTOTYPES
#include <GL3/gl3.h>

#include <malloc.h>
#include <strings.h>

#include "sine_wave.h"

char * rsxgltest_name = "buffer";

extern void tcp_printf(const char * fmt,...);

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

GLuint buffers[2];

static void
error_report()
{
  GLenum e = glGetError();
  if(e != GL_NO_ERROR) {
    tcp_printf("err:%x\n",e);
  }
  else {
    tcp_printf("no error\n");
  }
}

static void
label_error_report(const char * label)
{
  GLenum e = glGetError();
  if(e != GL_NO_ERROR) {
    tcp_printf("%s err:%x\n",label,e);
  }
  else {
    tcp_printf("%s no error\n",label);
  }
}

static void
_buffer_report(GLenum target)
{
  GLint size, usage, access, mapped;
  glGetBufferParameteriv(target,GL_BUFFER_SIZE,&size);
  glGetBufferParameteriv(target,GL_BUFFER_USAGE,&usage);
  glGetBufferParameteriv(target,GL_BUFFER_ACCESS,&access);
  glGetBufferParameteriv(target,GL_BUFFER_MAPPED,&mapped);
  
  tcp_printf("\tsize: %u\n",size);
  tcp_printf("\tusage: %x\n",usage);
  tcp_printf("\taccess: %x\n",access);
  tcp_printf("\tmapped: %u\n",mapped);
  
  if(mapped) {
    GLint mapped_offset, mapped_length;
    glGetBufferParameteriv(target,GL_BUFFER_MAP_OFFSET,&mapped_offset);
    glGetBufferParameteriv(target,GL_BUFFER_MAP_LENGTH,&mapped_length);
    tcp_printf("\tmapped offset, length: %u, %u\n",mapped_offset,mapped_length);
  }
}

static void
buffer_report(GLuint buf)
{
  tcp_printf("%s: %u\n",__PRETTY_FUNCTION__,buf);

  if(glIsBuffer(buf)) {
    glBindBuffer(GL_ARRAY_BUFFER,buf);

    GLenum e = glGetError();

    if(e == GL_NO_ERROR) {
      _buffer_report(GL_ARRAY_BUFFER);
    }
    else {
      tcp_printf("\terror binding buffer: %x\n",e);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER,0);
  }
  else {
    tcp_printf("\tnot a buffer\n");
  }
}

void
rsxgltest_init(int argc,const char ** argv)
{
  glClearColor(1,1,0,1);

  buffer_report(42);

  glGenBuffers(2,buffers);
  buffer_report(buffers[0]);
  buffer_report(buffers[1]);

  //
  void * tmp = malloc(1024 * 1024);

  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
  _buffer_report(GL_ARRAY_BUFFER);
  glBufferData(GL_ARRAY_BUFFER,1024 * 1024,0,GL_STATIC_DRAW);
  label_error_report("glBufferData");

  glBufferSubData(GL_ARRAY_BUFFER,0,1024 * 1024,tmp);
  label_error_report("glBufferSubData");

  glBufferSubData(GL_ARRAY_BUFFER,512 * 1024,1024 * 1024,tmp);
  label_error_report("glBufferSubData (out of bounds)");

  glBindBuffer(GL_ARRAY_BUFFER,0);

  free(tmp);

  glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);
  glBufferData(GL_ARRAY_BUFFER,512 * 1023,0,GL_STATIC_DRAW);
  _buffer_report(GL_ARRAY_BUFFER);
  label_error_report("glBufferData [1]");
  glBindBuffer(GL_ARRAY_BUFFER,0);

  //
  tmp = glMapBuffer(GL_ARRAY_BUFFER,GL_READ_WRITE);
  label_error_report("glMapBuffer (unbound)");
  tcp_printf("\tpointer: %x\n",(uint32_t)(uint64_t)tmp);

  glUnmapBuffer(GL_ARRAY_BUFFER);
  label_error_report("glUnmapBuffer (unbound)");

  //
  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);

  tmp = glMapBuffer(GL_ARRAY_BUFFER,GL_READ_WRITE);
  label_error_report("glMapBuffer");
  _buffer_report(GL_ARRAY_BUFFER);
  tcp_printf("\tpointer: %x\n",(uint32_t)(uint64_t)tmp);

  void * tmp2 = glMapBuffer(GL_ARRAY_BUFFER,GL_READ_WRITE);
  label_error_report("glMapBuffer (already mapped)");
  _buffer_report(GL_ARRAY_BUFFER);
  tcp_printf("\tpointer: %x\n",(uint32_t)(uint64_t)tmp2);

  if(tmp != 0) {
    bzero(tmp,1024 * 1024);
  }

  glUnmapBuffer(GL_ARRAY_BUFFER);
  label_error_report("glUnmapBuffer");
  _buffer_report(GL_ARRAY_BUFFER);

  tmp = glMapBufferRange(GL_ARRAY_BUFFER,1024 * 512,1024 * 1024,GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
  label_error_report("glMapBufferRange (out of bounds)");
  _buffer_report(GL_ARRAY_BUFFER);

  tmp = glMapBufferRange(GL_ARRAY_BUFFER,1024 * 512,256 * 1024,GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
  label_error_report("glMapBufferRange");
  _buffer_report(GL_ARRAY_BUFFER);
  tcp_printf("\tpointer: %x\n",(uint32_t)(uint64_t)tmp);

  if(tmp != 0) {
    bzero(tmp,256 * 1024);
  }

  glUnmapBuffer(GL_ARRAY_BUFFER);
  label_error_report("glUnmapBuffer");
  _buffer_report(GL_ARRAY_BUFFER);

  glBindBuffer(GL_ARRAY_BUFFER,0);

  //
  glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);
  tmp = glMapBuffer(GL_ARRAY_BUFFER,GL_READ_WRITE);
  if(tmp != 0) {
    bzero(tmp,1023 * 512);
  }
  glUnmapBuffer(GL_ARRAY_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER,0);

  glDeleteBuffers(2,buffers);

  buffer_report(buffers[0]);
  buffer_report(buffers[1]);
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
