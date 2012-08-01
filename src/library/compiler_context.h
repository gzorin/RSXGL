#ifndef compiler_context_H
#define compiler_context_H

#include <utility>

struct pipe_context;
struct st_context;
struct gl_shader;
struct gl_shader_program;
struct nvfx_vertex_program;
struct nvfx_fragment_program;
struct pipe_stream_output_info;
struct tgsi_token;

struct compiler_context_t {
  enum type {
    kVertex,
    kFragment,
    kGeometry
  };

  struct gl_context * mesa_ctx;
  
  compiler_context_t(pipe_context *);
  ~compiler_context_t();

  struct gl_shader * create_shader(const type);
  void compile_shader(struct gl_shader *,const char *);
  void destroy_shader(struct gl_shader *);

  struct gl_shader_program * create_program();
  void attach_shader(struct gl_shader_program *,struct gl_shader *);
  void bind_attrib_location(struct gl_shader_program *,unsigned int,const char *);
  void bind_frag_data_location(struct gl_shader_program *,unsigned int,const char *);
  void transform_feedback_varyings(struct gl_shader_program *,unsigned int,const char **,unsigned int);
  void link_program(struct gl_shader_program *);
  void destroy_program(struct gl_shader_program *);
  
  struct nvfx_vertex_program * translate_vp(struct gl_shader_program *,struct pipe_stream_output_info *,struct tgsi_token **);
  void destroy_vp(nvfx_vertex_program *);
  
  struct nvfx_fragment_program * translate_fp(struct gl_shader_program *);

  std::pair< struct nvfx_vertex_program *,struct nvfx_fragment_program * > translate_stream_vp_fp(struct gl_shader_program *,struct pipe_stream_output_info *,struct tgsi_token *,unsigned int *,unsigned int *);

  void link_vp_fp(struct nvfx_vertex_program *,struct nvfx_fragment_program *);

  void destroy_fp(nvfx_fragment_program *);
};

#endif
