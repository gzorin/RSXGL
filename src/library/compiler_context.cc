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

namespace {

struct gl_shader *
new_shader(struct gl_context *ctx, GLuint name, GLenum type)
{
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
