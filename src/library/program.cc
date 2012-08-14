// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// program.cc - Functions pertaining to creating, compiling, and linking shaders and programs.

#include <GL3/gl3.h>

#include "debug.h"
#include "rsxgl_assert.h"
#include "rsxgl_context.h"
#include "error.h"
#include "gl_fifo.h"
#include "program.h"
#include "compiler_context.h"

#include <rsx/gcm_sys.h>
#include "nv40.h"

#define MSPACES 1
#define ONLY_MSPACES 1
#define HAVE_MMAP 0
#define malloc_getpagesize 4096
#include "malloc-2.8.4.h"

// mesa:
#include <main/mtypes.h>
#include <state_tracker/st_program.h>
#include <program/prog_parameter.h>
#include <glsl/ir.h>
#include <glsl/ir_uniform.h>

extern "C" {
#include <nvfx/nvfx_state.h>
}

#include <malloc.h>
#include <algorithm>
#include <deque>
#include <map>
#include "set_algorithm2.h"

static u32 endian_fp(u32 v)
{
  return ( ( ( v >> 16 ) & 0xffff ) << 0 ) |
         ( ( ( v >> 0 ) & 0xffff ) << 16 );
}

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

//
shader_t::storage_type & shader_t::storage()
{
  return current_object_ctx() -> shader_storage();
}

program_t::storage_type & program_t::storage()
{
  return current_object_ctx() -> program_storage();
}

// Shader functions:
shader_t::shader_t()
  : type(RSXGL_MAX_SHADER_TYPES), compiled(GL_FALSE), deleted(GL_FALSE), ref_count(0), mesa_shader(0)
{
  source().construct();
  binary().construct();
  info().construct();
}

shader_t::~shader_t()
{
  source().destruct();
  binary().destruct();
  info().destruct();
}

static inline uint32_t 
rsxgl_shader_type(GLenum type)
{
  switch(type) {
  case GL_VERTEX_SHADER:
    return RSXGL_VERTEX_SHADER;
  case GL_FRAGMENT_SHADER:
    return RSXGL_FRAGMENT_SHADER;
  default:
    return RSXGL_MAX_SHADER_TYPES;
  };
}

GLAPI GLuint APIENTRY
glCreateShader (GLenum type)
{
  uint32_t rsx_type = rsxgl_shader_type(type);
  if(rsx_type == RSXGL_MAX_SHADER_TYPES) {
    RSXGL_ERROR(GL_INVALID_ENUM,0);
  }

  uint32_t name = shader_t::storage().create_name_and_object();
  shader_t::storage().at(name).type = rsx_type;

  shader_t & shader = shader_t::storage().at(name);
  compiler_context_t * cctx = current_ctx() -> compiler_context();
  shader.mesa_shader = cctx -> create_shader(rsx_type == RSXGL_VERTEX_SHADER ? compiler_context_t::kVertex : compiler_context_t::kFragment);

  RSXGL_NOERROR(name);
}

GLAPI void APIENTRY
glDeleteShader (GLuint shader_name)
{
  if(!shader_t::storage().is_object(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  
  shader_t::gl_object_type::maybe_delete(shader_name);

  // TODO: destroy mesa resources

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsShader (GLuint shader_name)
{
  RSXGL_NOERROR((shader_t::storage().is_object(shader_name)) ? GL_TRUE : GL_FALSE);
}

GLAPI void APIENTRY
glGetShaderiv (GLuint shader_name, GLenum pname, GLint *params)
{
  if(!shader_t::storage().is_object(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const shader_t & shader = shader_t::storage().at(shader_name);

  if(pname == GL_SHADER_TYPE) {
    if(shader.type == RSXGL_VERTEX_SHADER) {
      *params = GL_VERTEX_SHADER;
    }
    else if(shader.type == RSXGL_FRAGMENT_SHADER) {
      *params = GL_FRAGMENT_SHADER;
    }
  }
  else if(pname == GL_DELETE_STATUS) {
    *params = shader.deleted;
  }
  else if(pname == GL_COMPILE_STATUS) {
    *params = shader.compiled;
  }
  else if(pname == GL_INFO_LOG_LENGTH) {
    *params = shader.info_size - 1;
  }
  else if(pname == GL_SHADER_SOURCE_LENGTH) {
    *params = shader.source_size - 1;
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetShaderInfoLog (GLuint shader_name, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
  if(!shader_t::storage().is_object(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const shader_t & shader = shader_t::storage().at(shader_name);

  shader.info().get(infoLog,bufSize);
  if(length != 0) *length = shader.info_size;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetShaderSource (GLuint shader_name, GLsizei bufSize, GLsizei *length, GLchar *source)
{
  if(!shader_t::storage().is_object(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const shader_t & shader = shader_t::storage().at(shader_name);
  shader.source().get(source,bufSize);
  if(length != 0) *length = shader.source_size;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glShaderBinary (GLsizei n, const GLuint* shader_names, GLenum binaryformat, const GLvoid* binary, GLsizei length)
{
  if(n < 0 || length < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  for(GLsizei i = 0;i < n;++i,++shader_names) {
    if(!shader_t::storage().is_object(*shader_names)) {
      RSXGL_ERROR_(GL_INVALID_OPERATION);
    }

    shader_t & shader = shader_t::storage().at(*shader_names);

    if(binary != 0) {
      shader.binary().resize(length,0);
      shader.binary().set(binary,length);
    }
    else {
      shader.binary().resize(0);
    }

    shader.info().resize(0);
  }
  
  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glShaderSource (GLuint shader_name, GLsizei count, const GLchar** string, const GLint* length)
{
  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(!shader_t::storage().is_object(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  // Concatenate the source:
  std::string source;

  if(string != 0) {
    for(GLsizei i = 0;i < count;++i,++string) {
      if(*string == 0) continue;
      
      if(length != 0) {
	source.append(*string,length[i]);
      }
      else {
	source.append(*string);
      }

      static std::string kNewline("\n");
      source.append(kNewline);
    }
  }

  shader_t & shader = shader_t::storage().at(shader_name);
  if(source.length() > 0) {
    const size_t n = source.length() + 1;
    shader.source().resize(n,0);
    shader.source().set(source.c_str(),n);
  }
  else {
    shader.source().resize(0);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glCompileShader (GLuint shader_name)
{
  if(!shader_t::storage().is_object(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  shader_t & shader = shader_t::storage().at(shader_name);
  shader.compiled = GL_FALSE;

  if(shader.source_data == 0) {
    RSXGL_NOERROR_();
  }

  compiler_context_t * cctx = current_ctx() -> compiler_context();
  rsxgl_assert(cctx != 0);

  cctx -> compile_shader(shader.mesa_shader,
			 shader.source_data);

  shader.compiled = shader.mesa_shader -> CompileStatus;
  const size_t n = strlen(shader.mesa_shader -> InfoLog) + 1;
  shader.info().resize(n);
  shader.info().set(shader.mesa_shader -> InfoLog,n);

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glReleaseShaderCompiler (void)
{
  // TODO: Return an error, because we don't really support this yet:
  //RSXGL_NOERROR_();
  RSXGL_ERROR_(GL_INVALID_OPERATION);
}

// Program functions:
program_t::program_t()
  : deleted(0), timestamp(0),
    linked(0), validated(0), invalid_uniforms(0), ref_count(0),
    attrib_name_max_length(0), uniform_name_max_length(0),
    mesa_program(0), nvfx_vp(0), nvfx_fp(0), nvfx_streamvp(0), nvfx_streamfp(0),
    vp_ucode_offset(~0), fp_ucode_offset(~0), vp_num_insn(0), fp_num_insn(0), 
    streamvp_ucode_offset(~0), streamfp_ucode_offset(~0), streamvp_num_insn(0), streamfp_num_insn(0), 
    vp_input_mask(0), vp_output_mask(0), vp_num_internal_const(0),
    fp_control(0),
    streamvp_input_mask(0), streamvp_output_mask(0), streamvp_num_internal_const(0),
    streamfp_control(0), streamfp_num_outputs(0),
    streamvp_vertexid_index(~0), instanceid_index(~0), point_sprite_control(0),
    uniform_values(0), program_offsets(0)
{
  attached_shaders().construct();
  linked_shaders().construct();

  info().construct();
  names().construct();

  attrib_table().construct();
  uniform_table().construct();
  sampler_uniform_table().construct();
}

program_t::~program_t()
{
  attached_shaders().destruct();
  for(unsigned int i = 0,n = attached_shaders().get_size();i < n;++i) {
    shader_t::gl_object_type::unref_and_maybe_delete(attached_shaders()[i]);
  }

  linked_shaders().destruct();
  for(unsigned int i = 0,n = linked_shaders().get_size();i < n;++i) {
    shader_t::gl_object_type::unref_and_maybe_delete(linked_shaders()[i]);
  }

  info().destruct();
  names().destruct();

  attrib_table().destruct();
  uniform_table().destruct();
  sampler_uniform_table().destruct();

  if(uniform_values != 0) free(uniform_values);
  if(program_offsets != 0) free(program_offsets);

  // TODO: delete microcode storage also
}

GLAPI GLuint APIENTRY
glCreateProgram (void)
{
  uint32_t name = program_t::storage().create_name_and_object();

  program_t & program = program_t::storage().at(name);
  compiler_context_t * cctx = current_ctx() -> compiler_context();
  program.mesa_program = cctx -> create_program();

  RSXGL_NOERROR(name);
}

GLAPI void APIENTRY
glDeleteProgram (GLuint program_name)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  // TODO: orphan it, instead of doing this:
  program_t & program = program_t::storage().at(program_name);
  if(program.timestamp > 0) {
    rsxgl_timestamp_wait(current_ctx(),program.timestamp);
    program.timestamp = 0;
  }

  program_t::gl_object_type::maybe_delete(program_name);

  // TODO: destroy mesa resources
  
  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsProgram (GLuint program_name)
{
  RSXGL_NOERROR((program_t::storage().is_object(program_name) && program_t::storage().is_object(program_name)) ? GL_TRUE : GL_FALSE);
}

GLAPI void APIENTRY
glAttachShader (GLuint program_name, GLuint shader_name)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  if(!shader_t::storage().is_object(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  program_t & program = program_t::storage().at(program_name);
  shader_t & shader = shader_t::storage().at(shader_name);

  if(!program.attached_shaders().insert(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }
  else {
    shader_t::gl_object_type::ref(shader_name);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDetachShader (GLuint program_name, GLuint shader_name)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  if(!shader_t::storage().is_object(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  program_t & program = program_t::storage().at(program_name);
  shader_t & shader = shader_t::storage().at(shader_name);

  if(!program.attached_shaders().erase(shader_name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }
  else {
    //compiler_context_t * cctx = current_ctx() -> compiler_context();
    //cctx -> dettach_shader(program.mesa_program,shader.mesa_shader);

    shader_t::gl_object_type::unref_and_maybe_delete(shader_name);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetAttachedShaders (GLuint program_name, GLsizei maxCount, GLsizei *count, GLuint *obj)
{
  if(maxCount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const program_t & program = program_t::storage().at(program_name);

  size_t m = 0;
  for(size_t i = 0,n = std::min((size_t)maxCount,(size_t)program.attached_shaders().get_size());i < n;++i) {
    *obj++ = program.attached_shaders()[i];
    ++m;
  }

  if(count != 0) {
    *count = m;
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetProgramiv (GLuint program_name, GLenum pname, GLint *params)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const program_t & program = program_t::storage().at(program_name);

  if(pname == GL_DELETE_STATUS) {
    *params = program.deleted;
  }
  else if(pname == GL_LINK_STATUS) {
    *params = program.linked;
  }
  else if(pname == GL_VALIDATE_STATUS) {
    *params = program.validated;
  }
  else if(pname == GL_INFO_LOG_LENGTH) {
    *params = program.info_size - 1;
  }
  else if(pname == GL_ATTACHED_SHADERS) {
    // TODO: implement this
  }
  else if(pname == GL_ACTIVE_ATTRIBUTES) {
    if(program.linked) {
      *params = program.attrib_table_size;
    }
    else {
      *params = 0;
    }
  }
  else if(pname == GL_ACTIVE_ATTRIBUTE_MAX_LENGTH) {
    if(program.linked) {
      *params = program.attrib_name_max_length;
    }
    else {
      *params = 0;
    }
  }
  else if(pname == GL_ACTIVE_UNIFORMS) {
    if(program.linked) {
      *params = program.uniform_table_size + program.sampler_uniform_table_size;
    }
    else {
      *params = 0;
    }
  }
  else if(pname == GL_ACTIVE_UNIFORM_MAX_LENGTH) {
    if(program.linked) {
      *params = program.uniform_name_max_length;
    }
    else {
      *params = 0;
    }
  }
  else if(pname == GL_TRANSFORM_FEEDBACK_BUFFER_MODE) {
  }
  else if(pname == GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH) {
  }
  else if(pname == GL_GEOMETRY_VERTICES_OUT) {
  }
  else if(pname == GL_GEOMETRY_INPUT_TYPE) {
  }
  else if(pname == GL_GEOMETRY_OUTPUT_TYPE) {
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetProgramInfoLog (GLuint program_name, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const program_t & program = program_t::storage().at(program_name);
  program.info().get(infoLog,bufSize);
  if(length != 0) *length = program.info_size;

  RSXGL_NOERROR_();
}

//
static void * main_ucode_address = 0;

static inline struct nvfx_vertex_program_exec *
rsxgl_main_ucode_address(program_t::ucode_offset_type off)
{
  rsxgl_assert(off != ~0U);
  return (struct nvfx_vertex_program_exec *)((uint8_t *)main_ucode_address + (ptrdiff_t)off * sizeof(struct nvfx_vertex_program_exec));
}

static inline program_t::ucode_offset_type
rsxgl_vp_ucode_offset(const struct nvfx_vertex_program_exec * address)
{
  return ((uint8_t *)address - (uint8_t *)main_ucode_address) / (sizeof(struct nvfx_vertex_program_exec));
}

static mspace
rsxgl_main_ucode_mspace()
{
  static const size_t size = 1024 * 1024;
  static mspace space = 0;

  if(space == 0) {
    main_ucode_address = memalign(RSXGL_CACHE_LINE_SIZE,size);
    rsxgl_assert(main_ucode_address != 0);

    space = create_mspace_with_base(main_ucode_address,size,0);
    rsxgl_assert(space != 0);
  }

  return space;
}

//
void * rsx_ucode_address = 0;
uint32_t rsx_ucode_offset = 0;

static inline uint32_t *
rsxgl_rsx_ucode_address(program_t::ucode_offset_type off)
{
  rsxgl_assert(off != ~0U);
  return (uint32_t *)((uint8_t *)rsx_ucode_address + (ptrdiff_t)off * sizeof(uint32_t) * 4);
}

static inline uint32_t
rsxgl_rsx_ucode_offset(program_t::ucode_offset_type off)
{
  rsxgl_assert(off != ~0U);
  return (off * sizeof(uint32_t) * 4) + rsx_ucode_offset;
}

static inline program_t::ucode_offset_type
rsxgl_rsx_ucode_offset(const uint32_t * address)
{
  return ((uint8_t *)address - (uint8_t *)rsx_ucode_address) / (sizeof(uint32_t) * 4);
}

static mspace
rsxgl_rsx_ucode_mspace()
{
  static const size_t size = 4 * 1024 * 1024;
  static mspace space = 0;

  if(space == 0) {
    rsx_ucode_address = rsxgl_rsx_memalign(RSXGL_CACHE_LINE_SIZE,size);
    rsxgl_assert(rsx_ucode_address != 0);

    gcmAddressToOffset(rsx_ucode_address,&rsx_ucode_offset);

    space = create_mspace_with_base(rsx_ucode_address,size,0);
    rsxgl_assert(space != 0);
  }

  return space;
}

static inline uint8_t
rsxgl_glsl_type_to_rsxgl_type(const glsl_type * type)
{
  if(type -> base_type == GLSL_TYPE_FLOAT) {
    if(type -> vector_elements == 1) {
      return RSXGL_DATA_TYPE_FLOAT;
    }
    else if(type -> vector_elements == 2) {
      return RSXGL_DATA_TYPE_FLOAT2;
    }
    else if(type -> vector_elements == 3) {
      return RSXGL_DATA_TYPE_FLOAT3;
    }
    else if(type -> vector_elements == 4) {
      if(type -> matrix_columns == 4) {
	return RSXGL_DATA_TYPE_FLOAT4x4;
      }
      else {
	return RSXGL_DATA_TYPE_FLOAT4;
      }
    }
  }
  else if(type -> base_type == GLSL_TYPE_SAMPLER) {
    if(type -> sampler_dimensionality == GLSL_SAMPLER_DIM_1D) {
      return RSXGL_DATA_TYPE_SAMPLER1D;
    }
    else if(type -> sampler_dimensionality == GLSL_SAMPLER_DIM_2D) {
      return RSXGL_DATA_TYPE_SAMPLER2D;
    }
    else if(type -> sampler_dimensionality == GLSL_SAMPLER_DIM_3D) {
      return RSXGL_DATA_TYPE_SAMPLER3D;
    }
    else if(type -> sampler_dimensionality == GLSL_SAMPLER_DIM_CUBE) {
      return RSXGL_DATA_TYPE_SAMPLERCUBE;
    }
    else if(type -> sampler_dimensionality == GLSL_SAMPLER_DIM_RECT) {
      return RSXGL_DATA_TYPE_SAMPLERRECT;
    }
  }
  return RSXGL_DATA_TYPE_UNKNOWN;
}

GLAPI void APIENTRY
glLinkProgram (GLuint program_name)
{
  rsxgl_context_t * ctx = current_ctx();

  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if((ctx -> state.enable.transform_feedback_mode != 0) && (ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] == program_name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  program_t & program = program_t::storage().at(program_name);

  // TODO: orphan it, instead of doing this:
  if(program.timestamp > 0) {
    rsxgl_timestamp_wait(ctx,program.timestamp);
    program.timestamp = 0;
  }

  // Get rid of any linked shaders:
  for(unsigned int i = 0,n = program.linked_shaders().get_size();i < n;++i) {
    shader_t::gl_object_type::unref_and_maybe_delete(program.linked_shaders()[i]);
  }
  program.linked_shaders().destruct();

  // Destroy other tables, etc:
  if(program.vp_ucode_offset != ~0U) {
    mspace_free(rsxgl_main_ucode_mspace(),rsxgl_main_ucode_address(program.vp_ucode_offset));
    program.vp_ucode_offset = ~0U;
  }
  if(program.fp_ucode_offset != ~0U) {
    mspace_free(rsxgl_rsx_ucode_mspace(),rsxgl_rsx_ucode_address(program.fp_ucode_offset));
    program.fp_ucode_offset = ~0U;
  }
  if(program.streamvp_ucode_offset != ~0U) {
    mspace_free(rsxgl_main_ucode_mspace(),rsxgl_main_ucode_address(program.streamvp_ucode_offset));
    program.streamvp_ucode_offset = ~0U;
  }
  if(program.streamfp_ucode_offset != ~0U) {
    mspace_free(rsxgl_rsx_ucode_mspace(),rsxgl_rsx_ucode_address(program.streamfp_ucode_offset));
    program.streamfp_ucode_offset = ~0U;
  }
  program.names().destruct();
  program.attrib_table().destruct();
  program.uniform_table().destruct();
  program.sampler_uniform_table().destruct();
  if(program.program_offsets != 0) {
    free(program.program_offsets);
    program.program_offsets = 0;
  }
  if(program.uniform_values != 0) {
    free(program.uniform_values);
    program.uniform_values = 0;
  }

  program.linked = GL_FALSE;
  program.validated = GL_FALSE;

  //
  std::string info;

  compiler_context_t * cctx = ctx -> compiler_context();

  for(unsigned int i = 0,n = program.attached_shaders().get_size();i < n;++i) {
    const shader_t & shader = shader_t::storage().at(program.attached_shaders()[i]);

#if 0    
    char * tmp = (char *)malloc(shader.source_size);
    shader.source().get(tmp,shader.source_size);
    rsxgl_debug_printf("%u source:\n%s\n",i,tmp);
    free(tmp);
#endif

    cctx -> attach_shader(program.mesa_program,shader_t::storage().at(program.attached_shaders()[i]).mesa_shader);
  }
  
  cctx -> link_program(program.mesa_program);

#if 0
  rsxgl_debug_printf("%s result: %i info: %s programs: %lx %x\n",
		     __PRETTY_FUNCTION__,
		     program.mesa_program -> LinkStatus,
		     program.mesa_program -> InfoLog);
#endif

  info += std::string(program.mesa_program -> InfoLog);

  if(program.mesa_program -> LinkStatus) {
    pipe_stream_output_info stream_info;
    tgsi_token * vp_tokens = 0;

    program.nvfx_vp = cctx -> translate_vp(program.mesa_program,&stream_info,&vp_tokens);
    program.nvfx_fp = cctx -> translate_fp(program.mesa_program);
    rsxgl_assert(program.nvfx_vp != 0);
    rsxgl_assert(program.nvfx_fp != 0);

    cctx -> link_vp_fp(program.nvfx_vp,program.nvfx_fp);

    // Move attached shaders to linked shaders:
    program.linked_shaders_data = program.attached_shaders_data;
    program.linked_shaders_size = program.attached_shaders_size;

    // Start a new attached shaders array:
    program.attached_shaders().construct();

    {
      //
      // Migrate vertex program microcode to cache-aligned memory:
      {
	static const std::string kVPUcodeAllocFail("Failed to allocate space for vertex program microcode");
	
	struct nvfx_vertex_program_exec * address = (struct nvfx_vertex_program_exec *)mspace_memalign(rsxgl_main_ucode_mspace(),RSXGL_CACHE_LINE_SIZE,program.nvfx_vp -> nr_insns * sizeof(struct nvfx_vertex_program_exec));
	if(address == 0) {
	  info += kVPUcodeAllocFail;
	  //goto fail;
	}
	else {
	  program.vp_ucode_offset = rsxgl_vp_ucode_offset(address);
	  
	  memcpy(address,program.nvfx_vp -> insns,program.nvfx_vp -> nr_insns * sizeof(struct nvfx_vertex_program_exec));
	  
	  program.vp_num_insn = program.nvfx_vp -> nr_insns;
	  program.vp_input_mask = program.nvfx_vp -> ir;
	}
      }
      
      // Migrate fragment program microcode to RSX memory, performing endian swap along the way:
      {
	static const std::string kFPUcodeAllocFail("Failed to allocate space for fragment program microcode");
	
	uint32_t * address = (uint32_t *)mspace_memalign(rsxgl_rsx_ucode_mspace(),RSXGL_CACHE_LINE_SIZE,program.nvfx_fp -> insn_len * sizeof(uint32_t));
	if(address == 0) {
	  info += kFPUcodeAllocFail;
	  //goto fail;
	}
	else {
	  program.fp_ucode_offset = rsxgl_rsx_ucode_offset(address);
	  
	  //memcpy(address,program.nvfx_fp -> insn,program.nvfx_fp -> insn_len * sizeof(uint32_t));
	  for(unsigned int i = 0,n = program.nvfx_fp -> insn_len;i < n;++i) {
	    address[i] = endian_fp(program.nvfx_fp -> insn[i]);
	  }
	  
	  program.fp_num_insn = program.nvfx_fp -> insn_len / 4;
	  program.fp_control = program.nvfx_fp -> fp_control;
	}
      }

      program.vp_output_mask = program.nvfx_vp -> outregs | program.nvfx_fp -> outregs;
    }

    // Things that get accumulated:
    // program_offsets - uint32_t's
    // uniform_values - ieee32_t's
    // names_size - accumulate amount of space required for attribute & uniform names
    // attribs - map from string's to attrib_t's
    // uniforms - map from string's to uniform_t's
    // sampler_uniforms - map from string's to sampler_uniform_t's
    // then iterate over attribs, uniforms, sampler uniforms, create names area

    std::deque< uint32_t > program_offsets;
    std::deque< ieee32_t > uniform_values;

    struct cstr_less {
      bool operator()(const char * lhs,const char * rhs) const {
	return strcmp(lhs,rhs) < 0;
      }
    };

    std::map< const char *, program_t::attrib_t, cstr_less > attribs;
    std::map< const char *, program_t::uniform_t, cstr_less > uniforms;
    std::map< const char *, program_t::sampler_uniform_t, cstr_less > sampler_uniforms;
    program_t::name_size_type names_size = 0;

    //
    struct gl_shader * gl_vsh = program.mesa_program->_LinkedShaders[MESA_SHADER_VERTEX];
    struct gl_program * gl_vp = gl_vsh->Program;
    
    struct gl_shader * gl_fsh = program.mesa_program->_LinkedShaders[MESA_SHADER_FRAGMENT];
    struct gl_program * gl_fp = gl_fsh->Program;
    
    // Process vertex program attributes:
    {
      program.attrib_name_max_length = 0;
      
      struct st_vertex_program * st_vp = st_vertex_program((struct gl_vertex_program *)gl_vp);
      
      exec_list *ir = gl_vsh->ir;
      foreach_list(node, ir) {
	const ir_variable *const var = ((ir_instruction *) node)->as_variable();
	
	if (var == NULL
	    || var->mode != ir_var_in
	    || var->location == -1
	    || var->location < VERT_ATTRIB_GENERIC0)
	  continue;

	program_t::attrib_t attrib;
	attrib.type = rsxgl_glsl_type_to_rsxgl_type(var->type);
	attrib.index = st_vp -> input_to_index[var -> location];
	attrib.location = var -> location - VERT_ATTRIB_GENERIC0;

	attribs.insert(std::make_pair(var -> name,attrib));

	const program_t::name_size_type name_length = strlen(var -> name);
	program.attrib_name_max_length = std::max(program.attrib_name_max_length,(program_t::name_size_type)name_length);
	names_size += name_length + 1;
      }
    }
    
    // Process program uniforms:
    {
      // Build vp constant map - from index into gl_vp -> Parameters to hardware index:
      // Also deal with vertex program immediates:
      typedef std::map< unsigned int, uint32_t > nvfx_vp_constant_map_t;
      nvfx_vp_constant_map_t nvfx_vp_constant_map;
      uint32_t vp_num_internal_const = 0;
      
      if(program.nvfx_vp != 0) {
	const struct nvfx_vertex_program_data * vp_const = program.nvfx_vp -> consts;
	for(unsigned int i = 0,n = program.nvfx_vp -> nr_consts;i < n;++i,++vp_const) {
	  if(vp_const -> index == -1) {
	    program_offsets.push_back(1);
	    program_offsets.push_back(i);

	    for(unsigned int j = 0;j < 4;++j) {
	      ieee32_t tmp;
	      tmp.f = vp_const -> value[j];
	      uniform_values.push_back(tmp);
	    }

	    ++vp_num_internal_const;
	  }
	  else {
	    nvfx_vp_constant_map[vp_const -> index] = i;
	  }
	}
      }

      program.vp_num_internal_const = vp_num_internal_const;
      
      // Build fp constant map - from index into gl_fp -> Parameters to a std::deque of offsets:
      typedef std::map< unsigned int, std::deque< uint32_t > > nvfx_fp_constant_map_t;
      nvfx_fp_constant_map_t nvfx_fp_constant_map;
      
      if(program.nvfx_fp != 0) {
	const struct nvfx_fragment_program_data * fp_const = program.nvfx_fp -> consts;
	for(unsigned int i = 0,n = program.nvfx_fp -> nr_consts;i < n;++i,++fp_const) {
	  nvfx_fp_constant_map[fp_const -> index].push_back(fp_const -> offset);
	}
      }
      
#if 0
      rsxgl_debug_printf("%i uniforms:\n",program.mesa_program -> NumUserUniformStorage);
#endif

      for(unsigned int i = 0,n = program.mesa_program -> NumUserUniformStorage;i < n;++i) {
	const gl_uniform_storage * uniform_storage = program.mesa_program -> UniformStorage + i;
	const glsl_type * type = uniform_storage -> type;
	bool add_name = false;

#if 0
	rsxgl_debug_printf("\t%s type:%s num_driver_storage:%u\n",
			   uniform_storage -> name,
			   uniform_storage -> type -> name,
			   uniform_storage -> num_driver_storage);
#endif

	// Non-samplers:
	if(uniform_storage -> type -> base_type != GLSL_TYPE_SAMPLER) {
	  program_t::uniform_t uniform;
	  uniform.type = rsxgl_glsl_type_to_rsxgl_type(type);
	  uniform.count = type -> matrix_columns;

#if 0
	  rsxgl_debug_printf("\t\ttype:%u count:%u\n",(unsigned int)uniform.type,(unsigned int)uniform.count);
#endif

	  uniform.values_index = uniform_values.size();
	  std::fill_n(std::back_inserter(uniform_values),type -> vector_elements * type -> matrix_columns,ieee32_t());
	  
	  // Search for it in vp:
	  // store vp index
	  uniform.vp_index = 0;

	  for(unsigned int i = 0,n = gl_vp -> Parameters -> NumParameters;i < n;++i) {
	    gl_program_parameter * parameter = gl_vp -> Parameters -> Parameters + i;
	    if(parameter -> Type == PROGRAM_UNIFORM && strcmp(parameter -> Name,uniform_storage -> name) == 0) {
	      nvfx_vp_constant_map_t::const_iterator it = nvfx_vp_constant_map.find(i);
	      if(it != nvfx_vp_constant_map.end()) {
		uniform.enabled.set(RSXGL_VERTEX_SHADER);
		uniform.vp_index = it -> second;
	      }
	      break;
	    }
	  }
	  
	  // Search for it in fp:
	  // for each in count:
	  // - store an offset count n
	  // - store n (offsets / 4)
	  uniform.program_offsets_index = 0;

	  for(unsigned int i = 0,n = gl_fp -> Parameters -> NumParameters;i < n;++i) {
	    gl_program_parameter * parameter = gl_fp -> Parameters -> Parameters + i;
	    if(parameter -> Type == PROGRAM_UNIFORM && strcmp(parameter -> Name,uniform_storage -> name) == 0) {
	      nvfx_fp_constant_map_t::const_iterator it = nvfx_fp_constant_map.find(i);
	      if(it != nvfx_fp_constant_map.end()) {
		uniform.enabled.set(RSXGL_FRAGMENT_SHADER);
		uniform.program_offsets_index = program_offsets.size();
		
		const std::deque< uint32_t > & offsets = it -> second;
		program_offsets.push_back(offsets.size());

		for(std::deque< uint32_t >::const_iterator jt = offsets.begin(),jt_end = offsets.end();jt != jt_end;++jt) {
		  program_offsets.push_back(*jt / 4);
		}
	      }
	      break;
	    }
	  }

	  uniforms.insert(std::make_pair(uniform_storage -> name,uniform));
	  add_name = true;
	}
	// Sampler:
	else {
	  program_t::sampler_uniform_t sampler_uniform;
	  sampler_uniform.type = rsxgl_glsl_type_to_rsxgl_type(uniform_storage -> type);
	  sampler_uniform.vp_index = RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;
	  sampler_uniform.fp_index = RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;

	  // Search for it in vp:
	  for(unsigned int i = 0,n = gl_vp -> Parameters -> NumParameters;i < n;++i) {
	    gl_program_parameter * parameter = gl_vp -> Parameters -> Parameters + i;
	    if(parameter -> Type == PROGRAM_SAMPLER && strcmp(parameter -> Name,uniform_storage -> name) == 0) {
	      sampler_uniform.vp_index = (unsigned int)uniform_storage -> sampler;
	      break;
	    }
	  }
	  
	  // Search for it in fp:
	  for(unsigned int i = 0,n = gl_fp -> Parameters -> NumParameters;i < n;++i) {
	    gl_program_parameter * parameter = gl_fp -> Parameters -> Parameters + i;
	    if(parameter -> Type == PROGRAM_SAMPLER && strcmp(parameter -> Name,uniform_storage -> name) == 0) {
	      sampler_uniform.fp_index = (unsigned int)uniform_storage -> sampler;
	      break;
	    }
	  }

	  sampler_uniforms.insert(std::make_pair(uniform_storage -> name,sampler_uniform));
	  add_name = true;
	}

	if(add_name) {
	  const program_t::name_size_type name_length = strlen(uniform_storage -> name);
	  program.uniform_name_max_length = std::max(program.uniform_name_max_length,(program_t::name_size_type)name_length);
	  names_size += name_length + 1;
	}
      }
    }

    // Migrate program offsets array:
    rsxgl_assert(program.program_offsets == 0);
    program.program_offsets = (program_t::instruction_size_type *)memalign(RSXGL_CACHE_LINE_SIZE,sizeof(program_t::instruction_size_type) * program_offsets.size());
    std::copy(program_offsets.begin(),program_offsets.end(),program.program_offsets);  

    // Migrate uniform values array:
    rsxgl_assert(program.uniform_values == 0);
    program.uniform_values = (ieee32_t *)memalign(RSXGL_CACHE_LINE_SIZE,sizeof(ieee32_t) * uniform_values.size());
    std::copy(uniform_values.begin(),uniform_values.end(),program.uniform_values);

    // Make space for attribute and uniform names:
#if 0
    rsxgl_debug_printf("names require %u bytes\n",(unsigned int)names_size);
#endif
    program.names().resize(names_size);
    char * pnames = program.names_data;

    // Migrate attributes table:
    program.attribs_enabled.reset();

    if(attribs.size() > 0) {
#if 0
      rsxgl_debug_printf("%u attribs\n",attribs.size());
#endif

      program_t::attrib_table_type::type table = program.attrib_table();
      table.resize(attribs.size());

      unsigned int i = 0;
      for(std::map< const char *, program_t::attrib_t, cstr_less >::const_iterator jt = attribs.begin(),jt_end = attribs.end();jt != jt_end;++jt) {
#if 0
	rsxgl_debug_printf(" %s: type:%u index:%u\n",
			   jt -> first,
			   (unsigned int)jt -> second.type,
			   (unsigned int)jt -> second.index);
#endif
	
	table[i].first = pnames - program.names_data;
	table[i].second = jt -> second;
	
	program.attribs_enabled.set(jt -> second.index);
	program.attrib_assignments.set(jt -> second.index,jt -> second.location);
	
	for(const char * name = jt -> first;*name != 0;++name,++pnames) {
	  *pnames = *name;
	}
	*pnames++ = 0;
	
	++i;
      }
    }

    // Migrate uniforms table:
    if(uniforms.size() > 0) {
#if 0
      rsxgl_debug_printf("%u uniforms\n",uniforms.size());
#endif

      program_t::uniform_table_type::type table = program.uniform_table();
      table.resize(uniforms.size());

      unsigned int i = 0;
      for(std::map< const char *, program_t::uniform_t, cstr_less >::iterator it = uniforms.begin();
          it != uniforms.end(); it++) {
        const std::pair< const char *, program_t::uniform_t > value = *it;
#if 0
	rsxgl_debug_printf(" %s: type:%u count:%u values_index:%u vp_index:%u program_offsets_index:%u\n",
			   value.first,
			   (unsigned int)value.second.type,
			   (unsigned int)value.second.count,
			   (unsigned int)value.second.values_index,
			   (unsigned int)value.second.vp_index,
			   (unsigned int)value.second.program_offsets_index);
#endif

	table[i].first = pnames - program.names_data;
	table[i].second = value.second;

	for(const char * name = value.first;*name != 0;++name,++pnames) {
	  *pnames = *name;
	}
	*pnames++ = 0;

	++i;
      }
    }

    // Migrate texture table:
    program.fp_texcoords.reset();
    program.fp_texcoord2D.reset();
    program.fp_texcoord3D.reset();
    program.textures_enabled.reset();

    for(unsigned int i = 0;i < RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;++i) {
      program.texture_assignments.set(i,0);
    }

    if(sampler_uniforms.size() > 0) {
#if 0
      rsxgl_debug_printf("%u sampler uniforms\n",sampler_uniforms.size());
#endif

      program_t::sampler_uniform_table_type::type table = program.sampler_uniform_table();
      table.resize(sampler_uniforms.size());

      unsigned int i = 0;
      for(std::map< const char *, program_t::sampler_uniform_t, cstr_less >::iterator it = sampler_uniforms.begin();
          it != sampler_uniforms.end(); it++) {
        const std::pair< const char *, program_t::sampler_uniform_t > value = *it;
#if 0
	rsxgl_debug_printf(" %s: type:%u vp_index:%u fp_index:%u\n",
			   value.first,
			   (unsigned int)value.second.type,
			   (unsigned int)value.second.vp_index,
			   (unsigned int)value.second.fp_index);
#endif

	table[i].first = pnames - program.names_data;
	table[i].second = value.second;

	if(value.second.vp_index != RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
	  program.textures_enabled.set(value.second.vp_index);
	}
	if(value.second.fp_index != RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
	  program.fp_texcoords.set(value.second.fp_index);
	  if(value.second.type == RSXGL_DATA_TYPE_SAMPLER2D) {
	    program.fp_texcoord2D.set(value.second.fp_index);
	  }
	  else if(value.second.type == RSXGL_DATA_TYPE_SAMPLER3D) {
	    program.fp_texcoord3D.set(value.second.fp_index);
	  }
	  program.textures_enabled.set(RSXGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS + value.second.fp_index);
	}

	for(const char * name = value.first;*name != 0;++name,++pnames) {
	  *pnames = *name;
	}
	*pnames++ = 0;

	++i;
      }
    }

    {
      program_t::uniform_table_type::type table = program.uniform_table();

      const std::pair< bool, program_t::uniform_size_type > tmp = table.find(program.names(),"rsxgl_InstanceID");
      if(tmp.first) {
	program.instanceid_index = table[tmp.second].second.vp_index;
      }
      else {
	program.instanceid_index = ~0;
      }
    }

    // TODO: deal with this:
    program.point_sprite_control = 0;

    // Create stream programs if any varyings are captured.
    // This seems to clobber the original vertex program's data such that the main rendering program's
    // attribute assignments get messed up. This isn't good, but, for now, creating the stream programs
    // takes place after RSXGL is otherwise finished creating the rendering programs.
    if(stream_info.num_outputs > 0) {
#if 0
      rsxgl_debug_printf("VP stream outputs: %u\n",stream_info.num_outputs);
#endif

      unsigned int vertexid_index = 0;
      std::tie(program.nvfx_streamvp,program.nvfx_streamfp) = cctx -> translate_stream_vp_fp(program.mesa_program,&stream_info,vp_tokens,&vertexid_index);
      rsxgl_assert(program.nvfx_streamvp != 0);
      rsxgl_assert(program.nvfx_streamfp != 0);
      
      cctx -> link_vp_fp(program.nvfx_streamvp,program.nvfx_streamfp);

#if 0
      // Dump VP: microcode:
      {
	rsxgl_debug_printf("VP microcode: %u instructions\n",program.nvfx_streamvp -> nr_insns);
	for(unsigned int i = 0,n = program.nvfx_streamvp -> nr_insns;i < n;++i) {
	  rsxgl_debug_printf("%04u: %x %x %x %x\n",i,
			     program.nvfx_streamvp -> insns[i].data[0],
			     program.nvfx_streamvp -> insns[i].data[1],
			     program.nvfx_streamvp -> insns[i].data[2],
			     program.nvfx_streamvp -> insns[i].data[3]);
	}

	
      }
      
      // Dump FP microcode:
      {
	rsxgl_debug_printf("FP microcode: %u instructions\n",program.nvfx_streamfp -> insn_len / 4);
	for(unsigned int i = 0,n = program.nvfx_streamfp -> insn_len / 4;i < n;++i) {
	  rsxgl_debug_printf("%04u: %08x %08x %08x %08x\n",i,
			     program.nvfx_streamfp -> insn[i*4],
			     program.nvfx_streamfp -> insn[i*4+1],
			     program.nvfx_streamfp -> insn[i*4+2],
			     program.nvfx_streamfp -> insn[i*4+3]);
	}

	rsxgl_debug_printf("streamfp slots:\n");
	for(unsigned int i = 0;i < program.nvfx_streamfp -> num_slots;++i) {
	  rsxgl_debug_printf("\t%u: %u %u\n",i,
			     program.nvfx_streamfp -> slot_to_generic[i],
			     program.nvfx_streamvp -> generic_to_fp_input[program.nvfx_streamfp -> slot_to_generic[i]]);
	}
      }
#endif

      //
      // Migrate vertex program microcode to cache-aligned memory:
      {
	static const std::string kVPUcodeAllocFail("Failed to allocate space for stream vertex program microcode");
	
	struct nvfx_vertex_program_exec * address = (struct nvfx_vertex_program_exec *)mspace_memalign(rsxgl_main_ucode_mspace(),RSXGL_CACHE_LINE_SIZE,program.nvfx_streamvp -> nr_insns * sizeof(struct nvfx_vertex_program_exec));
	if(address == 0) {
	  info += kVPUcodeAllocFail;
	  //goto fail;
	}
	else {
	  program.streamvp_ucode_offset = rsxgl_vp_ucode_offset(address);
	  
	  memcpy(address,program.nvfx_streamvp -> insns,program.nvfx_streamvp -> nr_insns * sizeof(struct nvfx_vertex_program_exec));
	  
	  program.streamvp_num_insn = program.nvfx_streamvp -> nr_insns;
	  program.streamvp_input_mask = program.nvfx_streamvp -> ir;
	}
      }
      
      // Migrate fragment program microcode to RSX memory, performing endian swap along the way:
      {
	static const std::string kFPUcodeAllocFail("Failed to allocate space for stream fragment program microcode");
	
	uint32_t * address = (uint32_t *)mspace_memalign(rsxgl_rsx_ucode_mspace(),RSXGL_CACHE_LINE_SIZE,program.nvfx_streamfp -> insn_len * sizeof(uint32_t));
	if(address == 0) {
	  info += kFPUcodeAllocFail;
	  //goto fail;
	}
	else {
	  program.streamfp_ucode_offset = rsxgl_rsx_ucode_offset(address);
	  
	  //memcpy(address,program.nvfx_streamfp -> insn,program.nvfx_streamfp -> insn_len * sizeof(uint32_t));
	  for(unsigned int i = 0,n = program.nvfx_streamfp -> insn_len;i < n;++i) {
	    address[i] = endian_fp(program.nvfx_streamfp -> insn[i]);
	  }
	  
	  program.streamfp_num_insn = program.nvfx_streamfp -> insn_len / 4;
	  program.streamfp_control = program.nvfx_streamfp -> fp_control;
	  program.streamfp_num_outputs = stream_info.num_outputs;
	}
      }

      program.streamvp_output_mask = program.nvfx_streamvp -> outregs | program.nvfx_streamfp -> outregs;
      program.streamvp_vertexid_index = vertexid_index;
    }
    else {
      program.nvfx_streamvp = 0;
      program.nvfx_streamfp = 0;
      program.streamvp_ucode_offset = ~0;
      program.streamfp_ucode_offset = ~0;
      program.streamvp_num_insn = 0;
      program.streamfp_num_insn = 0;
      program.streamvp_input_mask = 0;
      program.streamvp_output_mask = 0;
      program.streamvp_num_internal_const = 0;
      program.streamfp_control = 0;
      program.streamfp_num_outputs = 0;
      program.streamvp_vertexid_index = ~0;
    }

    program.linked = GL_TRUE;

#if 0
    rsxgl_debug_printf("wrote %u names bytes\n",(unsigned int)(pnames - program.names_data));
#endif
  }
  
  if(info.length() > 0) {
    const size_t n = info.length() + 1;
    program.info().resize(n,0);
    program.info().set(info.c_str(),n);
  }
  else {
    program.info().resize(0);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glValidateProgram (GLuint program_name)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  program_t & program = program_t::storage().at(program_name);

  if(program.linked) {
    program.validated = GL_TRUE;
  }
  else {
    program.validated = GL_FALSE;
  }
  
  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glUseProgram (GLuint program_name)
{
  struct rsxgl_context_t * ctx = current_ctx();

  if(program_name != 0 && !program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> state.enable.transform_feedback_mode != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] != program_name) {
    const program_t::name_type prev_program_name = ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM];
    ctx -> program_binding.bind(RSXGL_ACTIVE_PROGRAM,program_name);
    ctx -> invalid.parts.program = 1;

    if(program_name != 0) {
      const program_t & program = program_t::storage().at(program_name);

      ctx -> state.enable.transform_feedback_program = (program.streamvp_num_insn > 0 && program.streamfp_num_insn > 0);
      
      if(prev_program_name != 0) {
	const program_t & prev_program = program_t::storage().at(prev_program_name);

	{
	  const program_t::attribs_bitfield_type
	    attribs_enabled = prev_program.attribs_enabled;
	  const program_t::attrib_assignments_type
	    prev_assignments = prev_program.attrib_assignments,
	    assignments = program.attrib_assignments;
	  
	  program_t::attribs_bitfield_type::const_iterator
	    enabled_it = attribs_enabled.begin();
	  program_t::attrib_assignments_type::const_iterator
	    prev_it = prev_assignments.begin(),
	    it = assignments.begin();
	  
	  for(program_t::attrib_size_type i = 0;i < program_t::attribs_bitfield_type::size;++i,enabled_it.next(attribs_enabled),prev_it.next(prev_assignments),it.next(assignments)) {
	    ctx -> invalid_attrib_assignments.set(i,enabled_it.test() && (prev_it.value() != it.value()));
	  }
	}

	{
	  const program_t::textures_bitfield_type
	    textures_enabled = prev_program.textures_enabled;
	  const program_t::texture_assignments_type
	    prev_assignments = prev_program.texture_assignments,
	    assignments = program.texture_assignments;
	  
	  program_t::textures_bitfield_type::const_iterator
	    enabled_it = textures_enabled.begin();
	  program_t::texture_assignments_type::const_iterator
	    prev_it = prev_assignments.begin(),
	    it = assignments.begin();
	  
	  for(program_t::texture_size_type i = 0;i < program_t::textures_bitfield_type::size;++i,enabled_it.next(textures_enabled),prev_it.next(prev_assignments),it.next(assignments)) {
	    ctx -> invalid_texture_assignments.set(i,enabled_it.test() && (prev_it.value() != it.value()));
	  }
	}
      }
      else {
	ctx -> invalid_attrib_assignments = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled;
	ctx -> invalid_texture_assignments = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].textures_enabled;
      }
    }
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glBindAttribLocation (GLuint program_name, GLuint index, const GLchar* name)
{
  if(index >= RSXGL_MAX_VERTEX_ATTRIBS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  program_t & program = program_t::storage().at(program_name);

  compiler_context_t * cctx = current_ctx() -> compiler_context();
  cctx -> bind_attrib_location(program.mesa_program,index,name);

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetActiveAttrib (GLuint program_name, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
  if(bufsize < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const program_t & program = program_t::storage().at(program_name);

  if(!program.linked) {
    if(length != 0) *length = 0;
    if(bufsize > 0) *name = 0;
    RSXGL_NOERROR_();
  }

  if(index >= program.attrib_table_size) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const char * attrib_name = program.names_data + program.attrib_table_values[index].first;

  size_t n = std::min((size_t)bufsize - 1,(size_t)strlen(attrib_name));
  strncpy(name,attrib_name,n);
  name[n] = 0;

  if(length != 0) *length = n;

  switch(program.attrib_table_values[index].second.type) {
  case RSXGL_DATA_TYPE_FLOAT:
    *type = GL_FLOAT;
    *size = 1;
    break;
  case RSXGL_DATA_TYPE_FLOAT2:
    *type = GL_FLOAT_VEC2;
    *size = 1;
    break;
  case RSXGL_DATA_TYPE_FLOAT3:
    *type = GL_FLOAT_VEC3;
    *size = 1;
    break;
  case RSXGL_DATA_TYPE_FLOAT4:
    *type = GL_FLOAT_VEC4;
    *size = 1;
    break;
  case RSXGL_DATA_TYPE_FLOAT4x4:
    *type = GL_FLOAT_MAT4;
    *size = 1;
    break;
  default:
    *type = GL_NONE;
    *size = 0;
  };

  RSXGL_NOERROR_();
}

GLAPI GLint APIENTRY
glGetAttribLocation (GLuint program_name, const GLchar* name)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR(GL_INVALID_VALUE,-1);
  }

  const program_t & program = program_t::storage().at(program_name);

  if(!program.linked) {
    RSXGL_NOERROR(-1);
  }

  std::pair< bool, program_t::attrib_size_type > tmp = program.attrib_table().find(program.names(),name);
  
  RSXGL_NOERROR((tmp.first) ? program.attrib_table_values[tmp.second].second.location : -1);
}

GLAPI void APIENTRY
glGetActiveUniform (GLuint program_name, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
  if(bufSize < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const program_t & program = program_t::storage().at(program_name);

  if(!program.linked) {
    if(length != 0) *length = 0;
    if(bufSize > 0) *name = 0;
    RSXGL_NOERROR_();
  }

  if(index >= (program.uniform_table_size + program.sampler_uniform_table_size)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const char * uniform_name = "";
  uint8_t uniform_type = ~0;

  if(index < program.uniform_table_size) {
    uniform_name = program.names_data + program.uniform_table_values[index].first;
    uniform_type = program.uniform_table_values[index].second.type;
  }
  else {
    const GLuint texture_index = index - program.uniform_table_size;

    uniform_name = program.names_data + program.sampler_uniform_table_values[texture_index].first;
    uniform_type = program.sampler_uniform_table_values[texture_index].second.type;
  }

  size_t n = std::min((size_t)bufSize - 1,(size_t)strlen(uniform_name));
  strncpy(name,uniform_name,n);
  name[n] = 0;

  if(length != 0) *length = n;

  switch(uniform_type) {
  case RSXGL_DATA_TYPE_FLOAT:
    *type = GL_FLOAT;
    rsxgl_assert(index < program.uniform_table_size);
    *size = program.uniform_table_values[index].second.count;
    break;
  case RSXGL_DATA_TYPE_FLOAT2:
    *type = GL_FLOAT_VEC2;
    rsxgl_assert(index < program.uniform_table_size);
    *size = program.uniform_table_values[index].second.count;
    break;
  case RSXGL_DATA_TYPE_FLOAT3:
    *type = GL_FLOAT_VEC3;
    rsxgl_assert(index < program.uniform_table_size);
    *size = program.uniform_table_values[index].second.count;
    break;
  case RSXGL_DATA_TYPE_FLOAT4:
    *type = GL_FLOAT_VEC4;
    rsxgl_assert(index < program.uniform_table_size);
    *size = program.uniform_table_values[index].second.count;
    break;
  case RSXGL_DATA_TYPE_FLOAT4x4:
    *type = GL_FLOAT_MAT4;
    rsxgl_assert(index < program.uniform_table_size);
    *size = program.uniform_table_values[index].second.count;
    break;
  case RSXGL_DATA_TYPE_SAMPLER1D:
    *type = GL_SAMPLER_1D;
    *size = 1;
  case RSXGL_DATA_TYPE_SAMPLER2D:
    *type = GL_SAMPLER_2D;
    *size = 1;
  case RSXGL_DATA_TYPE_SAMPLER3D:
    *type = GL_SAMPLER_3D;
    *size = 1;
  case RSXGL_DATA_TYPE_SAMPLERCUBE:
    *type = GL_SAMPLER_CUBE;
    *size = 1;
  case RSXGL_DATA_TYPE_SAMPLERRECT:
    *type = GL_SAMPLER_2D_RECT;
    *size = 1;
  default:
    *type = GL_NONE;
    *size = 0;
  };

  RSXGL_NOERROR_();
}

GLAPI int APIENTRY
glGetUniformLocation (GLuint program_name, const GLchar* name)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR(GL_INVALID_VALUE,-1);
  }

  const program_t & program = program_t::storage().at(program_name);

  if(!program.linked) {
    RSXGL_NOERROR(-1);
  }

  const std::pair< bool, program_t::uniform_size_type > tmp = program.uniform_table().find(program.names(),name);
  if(tmp.first) {
    RSXGL_NOERROR(tmp.second);
  }
  else {
    const std::pair< bool, program_t::texture_size_type > tmp = program.sampler_uniform_table().find(program.names(),name);
    RSXGL_NOERROR(tmp.first ? program.uniform_table_size + tmp.second : -1);
  }
}

GLAPI void APIENTRY
glBindFragDataLocation (GLuint program_name, GLuint color, const GLchar *name)
{
  if(color >= RSXGL_MAX_DRAW_BUFFERS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  program_t & program = program_t::storage().at(program_name);

  compiler_context_t * cctx = current_ctx() -> compiler_context();
  cctx -> bind_frag_data_location(program.mesa_program,color,name);
  
  RSXGL_NOERROR_();
}

GLAPI GLint APIENTRY
glGetFragDataLocation (GLuint program_name, const GLchar *name)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR(GL_INVALID_VALUE,-1);
  }

  program_t & program = program_t::storage().at(program_name);

  // TODO: implement this

  RSXGL_NOERROR(-1);
}

GLAPI void APIENTRY
glTransformFeedbackVaryings (GLuint program_name, GLsizei count, const GLchar* *varyings, GLenum bufferMode)
{
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(!(bufferMode == GL_INTERLEAVED_ATTRIBS || bufferMode == GL_SEPARATE_ATTRIBS)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  program_t & program = program_t::storage().at(program_name);

  compiler_context_t * cctx = current_ctx() -> compiler_context();
  cctx -> transform_feedback_varyings(program.mesa_program,count,varyings,bufferMode);
  
  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetTransformFeedbackVarying (GLuint program_name, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name)
{
  // TODO: implement this
  if(!program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  RSXGL_NOERROR_();
}

void
rsxgl_program_validate(rsxgl_context_t * ctx,const uint32_t timestamp)
{
  gcmContextData * context = ctx -> base.gcm_context;

  if(ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] != 0) {
    program_t & program = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM];

    rsxgl_assert(timestamp >= program.timestamp);
    program.timestamp = timestamp;    
  }

  if(ctx -> invalid.parts.program) {
    if(ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] != 0) {
      program_t & program = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM];
      
      if(program.linked) {
	// load the vertex program:
	{
	  uint32_t * buffer = gcm_reserve(context,program.vp_num_insn * 5 + 7);
	  
	  gcm_emit_method(&buffer,NV30_3D_VP_UPLOAD_FROM_ID,1);
	  gcm_emit(&buffer,0);
	  
	  const struct nvfx_vertex_program_exec * ucode = rsxgl_main_ucode_address(program.vp_ucode_offset);
	  for(size_t i = 0,n = program.vp_num_insn;i < n;++i,++ucode) {
	    gcm_emit_method(&buffer,NV30_3D_VP_UPLOAD_INST(0),4);
	    gcm_emit(&buffer,ucode -> data[0]);
	    gcm_emit(&buffer,ucode -> data[1]);
	    gcm_emit(&buffer,ucode -> data[2]);
	    gcm_emit(&buffer,ucode -> data[3]);
	  }
	  
	  gcm_emit_method(&buffer,NV30_3D_VP_START_FROM_ID,1);
	  gcm_emit(&buffer,0);
	  
	  gcm_emit_method(&buffer,NV40_3D_VP_ATTRIB_EN,2);
	  gcm_emit(&buffer,program.vp_input_mask);
	  gcm_emit(&buffer,program.vp_output_mask);
	  
	  gcm_finish_commands(context,&buffer);
	}
	
	// load vertex program internal constants:
	if(program.vp_num_internal_const > 0) {
	  const program_t::instruction_size_type * program_offsets = program.program_offsets;
	  const ieee32_t * uniform_values = program.uniform_values;
	  
	  for(program_t::uniform_size_type i = 0,n = program.vp_num_internal_const;i < n;++i) {
	    program_t::instruction_size_type count = *program_offsets++;
	    program_t::instruction_size_type index = *program_offsets++;
	    
	    uint32_t * buffer = gcm_reserve(context,6 * count);
	    
	    for(;count > 0;--count,++index,uniform_values += 4) {
	      gcm_emit_method(&buffer,NV30_3D_VP_UPLOAD_CONST_ID,5);
	      gcm_emit(&buffer,index);
	      
	      gcm_emit(&buffer,uniform_values[0].u);
	      gcm_emit(&buffer,uniform_values[1].u);
	      gcm_emit(&buffer,uniform_values[2].u);
	      gcm_emit(&buffer,uniform_values[3].u);
	    }
	    
	    gcm_finish_commands(context,&buffer);
	  }
	}
	
	// load the fragment program:
	{
	  uint32_t i = 0;
	  const uint32_t n = 4 + (2 * RSXGL_MAX_TEXTURE_COORDS);
	  
	  uint32_t * buffer = gcm_reserve(context,n);
	  
	  gcm_emit_method_at(buffer,i++,NV30_3D_FP_ACTIVE_PROGRAM,1);
	  gcm_emit_at(buffer,i++,rsxgl_rsx_ucode_offset(program.fp_ucode_offset) | NV30_3D_FP_ACTIVE_PROGRAM_DMA0);
	  
	  // Texcoord control:
#define  NV40TCL_TEX_COORD_CONTROL(x)                                   (0x00000b40+((x)*4))

#if 0
	  bit_set< RSXGL_MAX_TEXTURE_COORDS >::const_iterator it = program.fp_texcoords.begin(),
	    it2D = program.fp_texcoord2D.begin(),
	    it3D = program.fp_texcoord3D.begin();
	  uint32_t reg = 0xb40;
	  for(uint32_t j = 0;j < RSXGL_MAX_TEXTURE_COORDS;++j,i += 2) {
#if 0
	    const uint32_t cmds[2] = {
	      //NV40TCL_TEX_COORD_CONTROL(j),
	      reg,
	      it.test() ? 
	      ((it3D.test() ? (1 << 4) : 0) | (it2D.test() ? 1 : 0)) :
	      (0)
	    };
#endif
	    
#if 0
	    gcm_emit_method_at(buffer,i,cmds[0],1);
	    gcm_emit_at(buffer,i + 1,cmds[1]);
#endif
	    
	    gcm_emit_method_at(buffer,i,reg,1);
	    gcm_emit_at(buffer,i + 1,
			it.test() ? 
			((it3D.test() ? (1 << 4) : 0) | (it2D.test() ? 1 : 0)) :
			(0));
	    
	    it.next(program.fp_texcoords);
	    it2D.next(program.fp_texcoord2D);
	    it3D.next(program.fp_texcoord3D);
	    reg += 4;
	  }
#endif

	  uint32_t fp_texcoord_mask = program.vp_output_mask >> 14;
	  for(size_t j = 0;j < RSXGL_MAX_TEXTURE_COORDS;++j,fp_texcoord_mask >>= 1,i += 2) {
	    gcm_emit_method_at(buffer,i,NV40TCL_TEX_COORD_CONTROL(j),1);	    
	    //gcm_emit_at(buffer,i + 1,(fp_texcoord_mask & 0x1) ? ((1)) : 0);
	    //gcm_emit_at(buffer,i + 1,(fp_texcoord_mask & 0x1) ? ((1) | (1 << 4)) : 0);
	    //gcm_emit_at(buffer,i + 1,(((uint32_t)1) | ((uint32_t)0 << 4)));
	    //gcm_emit_at(buffer,i + 1,(((uint32_t)1) | ((uint32_t)1 << 4)));
	    gcm_emit_at(buffer,i + 1,0);
	  }

	  gcm_emit_method_at(buffer,i++,NV30_3D_FP_CONTROL,1);
	  gcm_emit_at(buffer,i++,program.fp_control);
	  
	  gcm_finish_n_commands(context,n);
	}
	
	// invalidate vertex program uniforms:
	if(program.uniform_table_size > 0) {
	  program.invalid_uniforms = 1;
	  program_t::uniform_table_type::value_type * uniform = program.uniform_table_values;
	  for(program_t::uniform_size_type i = 0,n = program.uniform_table_size;i < n;++i,++uniform) {
	    if(uniform -> second.enabled.test(RSXGL_VERTEX_SHADER)) {
	      uniform -> second.invalid.set(RSXGL_VERTEX_SHADER);
	    }
	  }
	}

#if 0	
	// Tell unused attributes to have a size of 0:
	{
	  const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > unused_attribs = ~program.attribs_enabled;
	  
	  if(unused_attribs.any()) {
	    uint32_t * buffer = gcm_reserve(context,2 * RSXGL_MAX_VERTEX_ATTRIBS);
	    size_t nbuffer = 0;
	    
	    for(size_t i = 0;i < RSXGL_MAX_VERTEX_ATTRIBS;++i) {
	      if(!unused_attribs.test(i)) continue;
	      
	      gcm_emit_method_at(buffer,nbuffer + 0,NV30_3D_VTXFMT(i),1);
	      gcm_emit_at(buffer,nbuffer + 1,
			  /* ((uint32_t)attribs.frequency[i] << 16 | */
			  ((uint32_t)0 << NV30_3D_VTXFMT_STRIDE__SHIFT) |
			  ((uint32_t)0 << NV30_3D_VTXFMT_SIZE__SHIFT) |
			  ((uint32_t)RSXGL_VERTEX_F32 & 0x7));
	      
	      nbuffer += 2;
	    }
	    
	    gcm_finish_n_commands(context,nbuffer);
	  }
	}
#endif
	
	// Set point sprite behavior:
	{
	  if(program.point_sprite_control == 0) {
	    uint32_t * buffer = gcm_reserve(context,2);
	    
	    gcm_emit_method_at(buffer,0,NV30_3D_POINT_PARAMETERS_ENABLE,1);
	    gcm_emit_at(buffer,1,0);
	    
	    gcm_finish_n_commands(context,2);
	  }
	  else {
	    //rsxgl_debug_printf("point sprite control: %x\n",program.point_sprite_control);
	    
	    uint32_t * buffer = gcm_reserve(context,4);
	    
	    gcm_emit_method_at(buffer,0,NV30_3D_POINT_PARAMETERS_ENABLE,1);
	    gcm_emit_at(buffer,1,1);
	    
	    gcm_emit_method_at(buffer,2,NV30_3D_POINT_SPRITE,1);
	    gcm_emit_at(buffer,3,program.point_sprite_control);
	    
	    gcm_finish_n_commands(context,4);
	  }
	}
      }
    }

    ctx -> invalid.parts.program = 0;
  }
}

// This function assumes that it's been called after rsxgl_program_validate for the same program.
// It therefore does not re-send uniform variable values. Also does not set texture control,
// because stream programs don't use textures.
void
rsxgl_feedback_program_validate(rsxgl_context_t * ctx,const uint32_t timestamp)
{
  gcmContextData * context = ctx -> base.gcm_context;

  if(ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] != 0) {
    program_t & program = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM];

    rsxgl_assert(timestamp >= program.timestamp);
    program.timestamp = timestamp;    
  }

  if(ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] != 0) {
    program_t & program = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM];
    
    if(program.linked) {
      // load the vertex program:
      {
	uint32_t * buffer = gcm_reserve(context,program.streamvp_num_insn * 5 + 7);
	
	gcm_emit_method(&buffer,NV30_3D_VP_UPLOAD_FROM_ID,1);
	gcm_emit(&buffer,0);

	const struct nvfx_vertex_program_exec * ucode = rsxgl_main_ucode_address(program.streamvp_ucode_offset);
	for(size_t i = 0,n = program.streamvp_num_insn;i < n;++i,++ucode) {
	  gcm_emit_method(&buffer,NV30_3D_VP_UPLOAD_INST(0),4);

	  gcm_emit(&buffer,ucode -> data[0]);
	  gcm_emit(&buffer,ucode -> data[1]);
	  gcm_emit(&buffer,ucode -> data[2]);
	  gcm_emit(&buffer,ucode -> data[3]);
	}
	
	gcm_emit_method(&buffer,NV30_3D_VP_START_FROM_ID,1);
	gcm_emit(&buffer,0);
	
	gcm_emit_method(&buffer,NV40_3D_VP_ATTRIB_EN,2);
	gcm_emit(&buffer,program.streamvp_input_mask);
	gcm_emit(&buffer,program.streamvp_output_mask);
	
	gcm_finish_commands(context,&buffer);
      }

#if 0      
      // load vertex program internal constants:
      if(program.streamvp_num_internal_const > 0) {
	const program_t::instruction_size_type * program_offsets = program.program_offsets;
	const ieee32_t * uniform_values = program.uniform_values;
	
	for(program_t::uniform_size_type i = 0,n = program.streamvp_num_internal_const;i < n;++i) {
	  program_t::instruction_size_type count = *program_offsets++;
	  program_t::instruction_size_type index = *program_offsets++;
	  
	  uint32_t * buffer = gcm_reserve(context,6 * count);
	  
	  for(;count > 0;--count,++index,uniform_values += 4) {
	    gcm_emit_method(&buffer,NV30_3D_VP_UPLOAD_CONST_ID,5);
	    gcm_emit(&buffer,index);
	    
	    gcm_emit(&buffer,uniform_values[0].u);
	    gcm_emit(&buffer,uniform_values[1].u);
	    gcm_emit(&buffer,uniform_values[2].u);
	    gcm_emit(&buffer,uniform_values[3].u);
	  }
	  
	  gcm_finish_commands(context,&buffer);
	}
      }
#endif
      
      // load the fragment program:
      {
	uint32_t i = 0;
	const uint32_t n = 4 /*+ (2 * RSXGL_MAX_TEXTURE_COORDS)*/;
	
	uint32_t * buffer = gcm_reserve(context,n);
	
	gcm_emit_method_at(buffer,i++,NV30_3D_FP_ACTIVE_PROGRAM,1);
	gcm_emit_at(buffer,i++,rsxgl_rsx_ucode_offset(program.streamfp_ucode_offset) | NV30_3D_FP_ACTIVE_PROGRAM_DMA0);

#if 0	
	// Texcoord control:
#define  NV40TCL_TEX_COORD_CONTROL(x)                                   (0x00000b40+((x)*4))
	bit_set< RSXGL_MAX_TEXTURE_COORDS >::const_iterator it = program.streamfp_texcoords.begin(),
	  it2D = program.streamfp_texcoord2D.begin(),
	  it3D = program.streamfp_texcoord3D.begin();
	uint32_t reg = 0xb40;
	for(uint32_t j = 0;j < RSXGL_MAX_TEXTURE_COORDS;++j,i += 2) {
#if 0
	  const uint32_t cmds[2] = {
	    //NV40TCL_TEX_COORD_CONTROL(j),
	    reg,
	    it.test() ? 
	    ((it3D.test() ? (1 << 4) : 0) | (it2D.test() ? 1 : 0)) :
	    (0)
	  };
	  
	  gcm_emit_method_at(buffer,i,cmds[0],1);
	  gcm_emit_at(buffer,i + 1,cmds[1]);
#endif
	  
	  gcm_emit_method_at(buffer,i,reg,1);
	  gcm_emit_at(buffer,i + 1,
		      it.test() ? 
		      ((it3D.test() ? (1 << 4) : 0) | (it2D.test() ? 1 : 0)) :
		      (0));
	  
	  it.next(program.streamfp_texcoords);
	  it2D.next(program.streamfp_texcoord2D);
	  it3D.next(program.streamfp_texcoord3D);
	  reg += 4;
	}
#endif
	
	gcm_emit_method_at(buffer,i++,NV30_3D_FP_CONTROL,1);
	gcm_emit_at(buffer,i++,program.streamfp_control);
	
	gcm_finish_n_commands(context,n);
      }
      
#if 0
      // invalidate vertex program uniforms:
      if(program.uniform_table_size > 0) {
	program.invalid_uniforms = 1;
	program_t::uniform_table_type::value_type * uniform = program.uniform_table_values;
	for(program_t::uniform_size_type i = 0,n = program.uniform_table_size;i < n;++i,++uniform) {
	  if(uniform -> second.enabled.test(RSXGL_VERTEX_SHADER)) {
	    uniform -> second.invalid.set(RSXGL_VERTEX_SHADER);
	  }
	}
      }
#endif
      
#if 0
      // Tell unused attributes to have a size of 0:
      {
	const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > unused_attribs = ~program.attribs_enabled;
	
	if(unused_attribs.any()) {
	  uint32_t * buffer = gcm_reserve(context,2 * RSXGL_MAX_VERTEX_ATTRIBS);
	  size_t nbuffer = 0;
	  
	  for(size_t i = 0;i < RSXGL_MAX_VERTEX_ATTRIBS;++i) {
	    if(!unused_attribs.test(i)) continue;
	    
	    gcm_emit_method_at(buffer,nbuffer + 0,NV30_3D_VTXFMT(i),1);
	    gcm_emit_at(buffer,nbuffer + 1,
			/* ((uint32_t)attribs.frequency[i] << 16 | */
			((uint32_t)0 << NV30_3D_VTXFMT_STRIDE__SHIFT) |
			((uint32_t)0 << NV30_3D_VTXFMT_SIZE__SHIFT) |
			((uint32_t)RSXGL_VERTEX_F32 & 0x7));
	    
	    nbuffer += 2;
	  }
	  
	  gcm_finish_n_commands(context,nbuffer);
	}
      }
#endif

#if 0      
      // Set point sprite behavior:
      {
	if(program.point_sprite_control == 0) {
	  uint32_t * buffer = gcm_reserve(context,2);
	  
	  gcm_emit_method_at(buffer,0,NV30_3D_POINT_PARAMETERS_ENABLE,1);
	  gcm_emit_at(buffer,1,0);
	  
	  gcm_finish_n_commands(context,2);
	}
	else {
	  //rsxgl_debug_printf("point sprite control: %x\n",program.point_sprite_control);
	  
	  uint32_t * buffer = gcm_reserve(context,4);
	  
	  gcm_emit_method_at(buffer,0,NV30_3D_POINT_PARAMETERS_ENABLE,1);
	  gcm_emit_at(buffer,1,1);
	  
	  gcm_emit_method_at(buffer,2,NV30_3D_POINT_SPRITE,1);
	  gcm_emit_at(buffer,3,program.point_sprite_control);
	  
	  gcm_finish_n_commands(context,4);
	}
      }
#endif
    }
  }
}
