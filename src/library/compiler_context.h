#ifndef compiler_context_H
#define compiler_context_H

struct pipe_context;
struct st_context;
struct gl_shader;
struct gl_shader_program;
struct nvfx_vertex_program;
struct nvfx_fragment_program;

struct compiler_context_t {
  enum type {
    kVertex,
    kFragment,
    kGeometry
  };

  struct gl_context * mesa_ctx;
  //struct st_context * mesa_st;
  
  compiler_context_t(pipe_context *);
  ~compiler_context_t();

  struct gl_shader * create_shader(const type);
  void compile_shader(struct gl_shader *,const char *);
  void destroy_shader(struct gl_shader *);

  struct gl_shader_program * create_program();
  void attach_shader(struct gl_shader_program *,struct gl_shader *);
  void link_program(struct gl_shader_program *);
  void destroy_program(struct gl_shader_program *);
  
  struct nvfx_vertex_program * translate_vp(struct gl_shader_program *);
  void destroy_vp(nvfx_vertex_program *);
  
  struct nvfx_fragment_program * translate_fp(struct gl_shader_program *);
  void destroy_fp(nvfx_fragment_program *);
};

#endif
