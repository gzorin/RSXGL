#ifndef compiler_context_H
#define compiler_context_H

struct pipe_context;
struct st_context;

struct compiler_context_t {
  struct gl_context * mesa_ctx;
  //struct st_context * mesa_st;
  
  compiler_context_t(pipe_context *);
  ~compiler_context_t();
};

#endif
