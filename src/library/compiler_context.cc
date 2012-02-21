#include "compiler_context.h"
#include "gl_constants.h"
#include "rsxgl_assert.h"
#include "debug.h"

extern "C" {
#include "main/mtypes.h"
#include "state_tracker/st_context.h"
#include "state_tracker/st_program.h"
#include "glsl/ralloc.h"
}

#include "glsl/ast.h"
#include "glsl/glsl_parser_extras.h"
#include "glsl/ir_optimization.h"
#include "glsl/ir_print_visitor.h"

namespace {

struct gl_shader *
new_shader(struct gl_context *ctx, GLuint name, GLenum type)
{
  rsxgl_debug_printf("%s\n",__PRETTY_FUNCTION__);

  struct gl_shader *shader;
  
  rsxgl_assert(type == GL_FRAGMENT_SHADER || type == GL_VERTEX_SHADER);
  shader = rzalloc(NULL, struct gl_shader);
  if (shader) {
    shader->Type = type;
    shader->Name = name;
    shader->RefCount = 1;
  }
  return shader;
}

/**
 * Called via ctx->Driver.NewProgram() to allocate a new vertex or
 * fragment program.
 */
struct gl_program *
new_program(struct gl_context *ctx, GLenum target, GLuint id)
{
  rsxgl_debug_printf("%s\n",__PRETTY_FUNCTION__);

  switch (target) {
  case GL_VERTEX_PROGRAM_ARB: {
    struct st_vertex_program *prog = ST_CALLOC_STRUCT(st_vertex_program);
    return _mesa_init_vertex_program(ctx, &prog->Base, target, id);
  }
    
  case GL_FRAGMENT_PROGRAM_ARB:
  case GL_FRAGMENT_PROGRAM_NV: {
    struct st_fragment_program *prog = ST_CALLOC_STRUCT(st_fragment_program);
    return _mesa_init_fragment_program(ctx, &prog->Base, target, id);
  }
    
  case MESA_GEOMETRY_PROGRAM: {
    struct st_geometry_program *prog = ST_CALLOC_STRUCT(st_geometry_program);
    return _mesa_init_geometry_program(ctx, &prog->Base, target, id);
  }
    
  default:
    rsxgl_assert(0);
    return NULL;
  }
}

/**
 * Called via ctx->Driver.ProgramStringNotify()
 * Called when the program's text/code is changed.  We have to free
 * all shader variants and corresponding gallium shaders when this happens.
 */
GLboolean
program_string_notify( struct gl_context *ctx,
		       GLenum target,
		       struct gl_program *prog )
{
  rsxgl_debug_printf("%s\n",__PRETTY_FUNCTION__);

  return GL_TRUE;
}

}

compiler_context_t::compiler_context_t(pipe_context * pipe)
{
  rsxgl_debug_printf("%s\n",__PRETTY_FUNCTION__);
  //mesa_context = st_create_context(API_OPENGL,pctx,0,0);

  // the mesa context:
  mesa_ctx = (struct gl_context *)calloc(1,sizeof(gl_context));
  memset(mesa_ctx,0,sizeof(*mesa_ctx));
  
  mesa_ctx -> API = API_OPENGL;

  mesa_ctx -> Extensions.dummy_false = false;
  mesa_ctx -> Extensions.dummy_true = true;
  mesa_ctx -> Extensions.ARB_ES2_compatibility = true;
  mesa_ctx -> Extensions.ARB_draw_instanced = true;
  mesa_ctx -> Extensions.ARB_fragment_coord_conventions = true;
  mesa_ctx -> Extensions.EXT_texture_array = true;
  mesa_ctx -> Extensions.NV_texture_rectangle = true;
  mesa_ctx -> Extensions.EXT_texture3D = true;
  mesa_ctx -> Extensions.OES_EGL_image_external = true;
  
  mesa_ctx -> Const.GLSLVersion = 130;
  
  /* 1.20 minimums. */
  mesa_ctx -> Const.MaxLights = 8;
  mesa_ctx -> Const.MaxClipPlanes = 6;
  mesa_ctx -> Const.MaxTextureUnits = RSXGL_MAX_TEXTURE_IMAGE_UNITS;
  mesa_ctx -> Const.MaxTextureCoordUnits = RSXGL_MAX_TEXTURE_COORDS;
  mesa_ctx -> Const.VertexProgram.MaxAttribs = RSXGL_MAX_VERTEX_ATTRIBS;
  
  mesa_ctx -> Const.VertexProgram.MaxUniformComponents = RSXGL__VERTEX__MAX_PROGRAM_UNIFORM_COMPONENTS;
  mesa_ctx -> Const.MaxVarying = 8; /* == gl_MaxVaryingFloats / 4 */
  mesa_ctx -> Const.MaxVertexTextureImageUnits = RSXGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS;
  mesa_ctx -> Const.MaxCombinedTextureImageUnits = RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;
  mesa_ctx -> Const.MaxTextureImageUnits = RSXGL_MAX_TEXTURE_IMAGE_UNITS;
  mesa_ctx -> Const.FragmentProgram.MaxUniformComponents = RSXGL__VERTEX__MAX_PROGRAM_UNIFORM_COMPONENTS;
  
  mesa_ctx -> Const.MaxDrawBuffers = RSXGL_MAX_DRAW_BUFFERS;

  mesa_ctx -> Driver.NewShader = new_shader;
  mesa_ctx -> Driver.NewProgram = new_program;
  mesa_ctx -> Driver.ProgramStringNotify = program_string_notify;
  
  // the mesa state tracker:
  struct st_context * mesa_st = ST_CALLOC_STRUCT(st_context);
  mesa_st -> ctx = mesa_ctx;
  mesa_st -> pipe = pipe;

  mesa_ctx -> st = mesa_st;
}

compiler_context_t::~compiler_context_t()
{
  //st_destroy_context(mesa_context);

  free(mesa_ctx -> st);
  free(mesa_ctx);
}

struct gl_shader *
compiler_context_t::compile_shader(const type t,const char * src)
{
  rsxgl_assert(mesa_ctx != 0);

  rsxgl_debug_printf("%u [%s]\n",(unsigned int)t,src);

  gl_shader * shader = rzalloc(0,gl_shader);
  rsxgl_assert(shader != 0);

  if(t == kVertex) {
    shader -> Type = GL_VERTEX_SHADER;
  }
  else if(t == kFragment) {
    shader -> Type = GL_FRAGMENT_SHADER;
  }
  else if(t == kGeometry) {
    shader -> Type = GL_GEOMETRY_SHADER;
  }
  else {
    ralloc_free(shader);
    return 0;
  }

  shader -> Source = src;

  struct _mesa_glsl_parse_state *state =
    new(shader) _mesa_glsl_parse_state(mesa_ctx, shader->Type, shader);

  int count = 0;
  
  const char *source = shader->Source;
  state->error = preprocess(state, &source, &state->info_log,
			    state->extensions, mesa_ctx->API) != 0;

  rsxgl_debug_printf("\t%i\n",count++);

  if (!state->error) {
    _mesa_glsl_lexer_ctor(state, source);
    _mesa_glsl_parse(state);
    _mesa_glsl_lexer_dtor(state);
  }

  rsxgl_debug_printf("\t%i\n",count++);
  
  shader->ir = new(shader) exec_list;
  if (!state->error && !state->translation_unit.is_empty())
    _mesa_ast_to_hir(shader->ir, state);

  rsxgl_debug_printf("\t%i\n",count++);
  
  /* Optimization passes */
  if (!state->error && !shader->ir->is_empty()) {
    bool progress;
    do {
      progress = do_common_optimization(shader->ir, false, false, 32);
      rsxgl_debug_printf("\t\t%i\n",(int)progress);
    } while (progress);

    rsxgl_debug_printf("\t%i\n",count++);
    
    validate_ir_tree(shader->ir);
  }

  rsxgl_debug_printf("\t%i\n",count++);
  
  shader->symbols = state->symbols;
  shader->CompileStatus = !state->error;
  shader->Version = state->language_version;
  memcpy(shader->builtins_to_link, state->builtins_to_link,
	 sizeof(shader->builtins_to_link[0]) * state->num_builtins_to_link);
  shader->num_builtins_to_link = state->num_builtins_to_link;

  rsxgl_debug_printf("\t%i\n",count++);
  
  if (shader->InfoLog)
    ralloc_free(shader->InfoLog);
  
  shader->InfoLog = state->info_log;

  rsxgl_debug_printf("\t%i\n",count++);
  
  /* Retain any live IR, but trash the rest. */
  reparent_ir(shader->ir, shader);

  rsxgl_debug_printf("\t%i\n",count++);
  
  ralloc_free(state);

  return shader;
}
