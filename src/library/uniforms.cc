// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// uniforms.cc - Set program uniform variables ("constants").

#include "debug.h"
#include "rsxgl_assert.h"
#include "rsxgl_context.h"
#include "gl_constants.h"
#include "uniforms.h"

#include <GL3/gl3.h>
#include "error.h"

#include "gcm.h"
#include "nv40.h"
#include "gl_fifo.h"

// Get a few things from cgcomp's headers:
#define __TYPES_H__
#define FLOAT32  0x0
#define FLOAT16  0x1
#define FIXED12  0x2
#define INLINE __inline
#define boolean char
#include "cgcomp/nv40_vertprog.h"
#undef FLOAT32
#undef FLOAT16
#undef FIXED12
#undef INLINE
#undef boolean
#undef __TYPES_H__

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

static inline void
set_gpu_data(ieee32_t & lhs,const int8_t rhs) {
  lhs.u = rhs;
}

static inline void
set_gpu_data(ieee32_t & lhs,const int16_t rhs) {
  lhs.u = rhs;
}

static inline void
set_gpu_data(ieee32_t & lhs,const int32_t rhs) {
  lhs.u = rhs;
}

//
static inline void
set_gpu_data(ieee32_t & lhs,const uint8_t rhs) {
  lhs.u = rhs;
}

static inline void
set_gpu_data(ieee32_t & lhs,const uint16_t rhs) {
  lhs.u = rhs;
}

static inline void
set_gpu_data(ieee32_t & lhs,const uint32_t rhs) {
  lhs.u = rhs;
}

static inline void
set_gpu_data(ieee32_t & lhs,const float rhs) {
  lhs.f = rhs;
}

//
static inline void
get_gpu_data(const ieee32_t & lhs,int8_t & rhs) {
  rhs = lhs.u;
}

static inline void
get_gpu_data(const ieee32_t & lhs,int16_t & rhs) {
  rhs = lhs.u;
}

static inline void
get_gpu_data(const ieee32_t & lhs,int32_t & rhs) {
  rhs = lhs.u;
}

//
static inline void
get_gpu_data(const ieee32_t & lhs,uint8_t & rhs) {
  rhs = lhs.u;
}

static inline void
get_gpu_data(const ieee32_t & lhs,uint16_t & rhs) {
  rhs = lhs.u;
}

static inline void
get_gpu_data(const ieee32_t & lhs,uint32_t & rhs) {
  rhs = lhs.u;
}

static inline void
get_gpu_data(const ieee32_t & lhs,float & rhs) {
  rhs = lhs.f;
}

template< typename Type, size_t Width, rsxgl_data_types RSXGLType >
static inline void
rsxgl_uniform(rsxgl_context_t * ctx,
	      program_t::name_type program_name,
	      GLint location,
	      const Type & v0 = 0,const Type & v1 = 0,const Type & v2 = 0,const Type & v3 = 0)
{
  if(program_name == 0 || !program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(location == -1) {    
    RSXGL_NOERROR_();
  }

  program_t & program = program_t::storage().at(program_name);

  if(location >= program.uniform_table_size) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  program_t::uniform_t & uniform = program.uniform_table_values[location].second;

  if(uniform.type != RSXGLType) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  program.invalid_uniforms = 1;
  uniform.invalid = uniform.enabled;

  ieee32_t * values = program.uniform_values + uniform.values_index;
  set_gpu_data(values[0],v0);
  if(Width > 1) set_gpu_data(values[1],v1);
  if(Width > 2) set_gpu_data(values[2],v2);
  if(Width > 3) set_gpu_data(values[3],v3);

  RSXGL_NOERROR_();
}

template< typename Type, size_t Width, size_t Height, rsxgl_data_types RSXGLType >
static inline void
rsxgl_uniform(rsxgl_context_t * ctx,
	      program_t::name_type program_name,
	      GLint location,GLsizei count,GLboolean transpose,
	      const Type * v)
{
  if(program_name == 0 || !program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(location == -1) {
    RSXGL_NOERROR_();
  }

  program_t & program = program_t::storage().at(program_name);

  if(location >= program.uniform_table_size) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  program_t::uniform_t & uniform = program.uniform_table_values[location].second;

  if(uniform.type != RSXGLType) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if((Height * count) > uniform.count) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  program.invalid_uniforms = 1;
  uniform.invalid = uniform.enabled;

  ieee32_t * values = program.uniform_values + uniform.values_index;

  //  rsxgl_debug_printf("%s: %u: ",__PRETTY_FUNCTION__,uniform.values_index);

  if(Height > 1 && transpose) {
    for(program_t::uniform_size_type n = count;n > 0;--n) {
      for(size_t i = 0;i < Width;++i) {
	for(size_t j = 0;j < Height;++j,++values) {
	  set_gpu_data(*values,v[(j * Width) + i]);
	}
      }
      v += Width * Height;
    }
  }
  else {
    for(program_t::uniform_size_type n = Width * Height * count;n > 0;--n,++values,++v) {
      //rsxgl_debug_printf("%f ",*v);
      set_gpu_data(*values,*v);
    }
  }

  //rsxgl_debug_printf("\n");

  RSXGL_NOERROR_();
}

static inline void
rsxgl_sampler_uniform(rsxgl_context_t * ctx,
		      program_t::name_type program_name,
		      GLint location,
		      const GLint & v0 = 0)
{
  if(program_name == 0 || !program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(location == -1) {
    RSXGL_NOERROR_();
  }

  program_t & program = program_t::storage().at(program_name);

  const GLint texture_location = location - program.uniform_table_size;

  if(texture_location < 0 || texture_location >= program.sampler_uniform_table_size) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  program_t::sampler_uniform_t & texture = program.sampler_uniform_table_values[texture_location].second;

  if(texture.vp_index != RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
    rsxgl_assert(program.textures_enabled.test(texture.vp_index));

    program.texture_assignments.set(texture.vp_index,v0);
    if(ctx -> program_binding.is_bound(RSXGL_ACTIVE_PROGRAM,program_name)) ctx -> invalid_texture_assignments.set(texture.vp_index);
  }
  if(texture.fp_index != RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
    rsxgl_assert(program.textures_enabled.test(RSXGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS + texture.fp_index));

    program.texture_assignments.set(RSXGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS + texture.fp_index,v0);
    if(ctx -> program_binding.is_bound(RSXGL_ACTIVE_PROGRAM,program_name)) ctx -> invalid_texture_assignments.set(RSXGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS + texture.fp_index);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glUniform1f (GLint location, GLfloat x)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_uniform< GLfloat, 1, RSXGL_DATA_TYPE_FLOAT > (ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,x);
}

GLAPI void APIENTRY
glUniform1fv (GLint location, GLsizei count, const GLfloat* v)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_uniform< GLfloat, 1, 1, RSXGL_DATA_TYPE_FLOAT > (ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,count,GL_FALSE,v);
}

GLAPI void APIENTRY
glUniform1i (GLint location, GLint x)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_sampler_uniform(ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,x);
}

GLAPI void APIENTRY
glUniform1iv (GLint location, GLsizei count, const GLint* v)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_sampler_uniform(ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,*v);
}

GLAPI void APIENTRY
glUniform2f (GLint location, GLfloat x, GLfloat y)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_uniform< GLfloat, 2, RSXGL_DATA_TYPE_FLOAT2 > (ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,x,y);
}

GLAPI void APIENTRY
glUniform2fv (GLint location, GLsizei count, const GLfloat* v)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_uniform< GLfloat, 2, 1, RSXGL_DATA_TYPE_FLOAT2 > (ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,count,GL_FALSE,v);
}

GLAPI void APIENTRY
glUniform2i (GLint location, GLint x, GLint y)
{
}

GLAPI void APIENTRY
glUniform2iv (GLint location, GLsizei count, const GLint* v)
{
}

GLAPI void APIENTRY
glUniform3f (GLint location, GLfloat x, GLfloat y, GLfloat z)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_uniform< GLfloat, 3, RSXGL_DATA_TYPE_FLOAT3 > (ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,x,y,z);
}

GLAPI void APIENTRY
glUniform3fv (GLint location, GLsizei count, const GLfloat* v)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_uniform< GLfloat, 3, 1, RSXGL_DATA_TYPE_FLOAT3 > (ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,count,GL_FALSE,v);
}

GLAPI void APIENTRY
glUniform3i (GLint location, GLint x, GLint y, GLint z)
{
}

GLAPI void APIENTRY
glUniform3iv (GLint location, GLsizei count, const GLint* v)
{
}

GLAPI void APIENTRY
glUniform4f (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_uniform< GLfloat, 4, RSXGL_DATA_TYPE_FLOAT4 > (ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,x,y,z,w);
}

GLAPI void APIENTRY
glUniform4fv (GLint location, GLsizei count, const GLfloat* v)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_uniform< GLfloat, 4, 1, RSXGL_DATA_TYPE_FLOAT4 > (ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,count,GL_FALSE,v);
}

GLAPI void APIENTRY
glUniform4i (GLint location, GLint x, GLint y, GLint z, GLint w)
{
}

GLAPI void APIENTRY
glUniform4iv (GLint location, GLsizei count, const GLint* v)
{
}

GLAPI void APIENTRY
glUniformMatrix2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
}

GLAPI void APIENTRY
glUniformMatrix3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
}

GLAPI void APIENTRY
glUniformMatrix4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
  rsxgl_context_t * ctx = current_ctx();
  rsxgl_uniform< GLfloat, 4, 4, RSXGL_DATA_TYPE_FLOAT4x4 > (ctx,ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM],location,count,GL_FALSE,value);
}

GLAPI void APIENTRY
glUniformMatrix2x3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
}

GLAPI void APIENTRY
glUniformMatrix3x2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
}

GLAPI void APIENTRY
glUniformMatrix2x4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
}

GLAPI void APIENTRY
glUniformMatrix4x2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
}

GLAPI void APIENTRY
glUniformMatrix3x4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
}

GLAPI void APIENTRY
glUniformMatrix4x3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
}

GLAPI void APIENTRY
glUniform1ui (GLint location, GLuint v0)
{
}

GLAPI void APIENTRY
glUniform2ui (GLint location, GLuint v0, GLuint v1)
{
}

GLAPI void APIENTRY
glUniform3ui (GLint location, GLuint v0, GLuint v1, GLuint v2)
{
}

GLAPI void APIENTRY
glUniform4ui (GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
}

GLAPI void APIENTRY
glUniform1uiv (GLint location, GLsizei count, const GLuint *value)
{
}

GLAPI void APIENTRY
glUniform2uiv (GLint location, GLsizei count, const GLuint *value)
{
}

GLAPI void APIENTRY
glUniform3uiv (GLint location, GLsizei count, const GLuint *value)
{
}

GLAPI void APIENTRY
glUniform4uiv (GLint location, GLsizei count, const GLuint *value)
{
}

GLAPI void APIENTRY
glGetUniformufv (GLuint program_name, GLint location, GLfloat *params)
{
  if(program_name == 0 || !program_t::storage().is_object(program_name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(location == -1) {
    RSXGL_NOERROR_();
  }

  program_t & program = program_t::storage().at(program_name);

  if(location >= program.uniform_table_size) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  const program_t::uniform_t & uniform = program.uniform_table_values[location].second;  

  program_t::uniform_size_type width = 0;
  switch(uniform.type) {
  case RSXGL_DATA_TYPE_FLOAT:
    width = 1;
    break;
  case RSXGL_DATA_TYPE_FLOAT2:
    width = 2;
    break;
  case RSXGL_DATA_TYPE_FLOAT3:
    width = 3;
    break;
  case RSXGL_DATA_TYPE_FLOAT4:
    width = 4;
    break;
  case RSXGL_DATA_TYPE_FLOAT4x4:
    width = 4;
    break;
  default:
    width = 0;
    break;
  }

  if(width == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  const ieee32_t * values = program.uniform_values + uniform.values_index;
  for(program_t::uniform_size_type i = 0,n = uniform.count * width;i < n;++i,++params,++values) {
    get_gpu_data(*values,*params);
  }
}

GLAPI void APIENTRY
glGetUniformiv (GLuint program, GLint location, GLuint *params)
{
  
}

GLAPI void APIENTRY
glGetUniformuiv (GLuint program, GLint location, GLuint *params)
{
  
}

// From program.cc:
extern rsx_ptr_t rsx_ucode_address;
extern uint32_t rsx_ucode_offset;

static inline program_t::fp_instruction_type *
rsxgl_rsx_ucode_address(program_t::ucode_offset_type off)
{
  rsxgl_assert(off != ~0U);
  return (program_t::fp_instruction_type *)((uint8_t *)rsx_ucode_address + (ptrdiff_t)off * sizeof(program_t::fp_instruction_type));
}

static inline uint32_t
rsxgl_rsx_ucode_offset(program_t::ucode_offset_type off)
{
  rsxgl_assert(off != ~0U);
  return (off * sizeof(program_t::fp_instruction_type)) + rsx_ucode_offset;
}

static inline void
rsxgl_inline_transfer(gcmContextData * context,const uint32_t offset,const uint32_t width,const ieee32_t * pvalues)
{
  const uint32_t offset_aligned = offset & ~0x3f;
  const uint32_t shift = (offset & 0x3f) >> 2;
  const uint32_t width_pad = (width + 1) & ~0x01;
  
  //rsxgl_debug_printf("\toffset: %u offset_aligned:%u shift:%u width_pad:%u\n",offset,offset_aligned,shift,width_pad);
  
  //
  uint32_t * buffer = gcm_reserve(context,12 + width_pad);
  
  gcm_emit_method(&buffer,NV3062TCL_SET_CONTEXT_DMA_IMAGE_DEST,1);
  gcm_emit(&buffer,0xFEED0000);
  
  gcm_emit_method(&buffer,NV3062TCL_SET_OFFSET_DEST,1);
  gcm_emit(&buffer,offset_aligned);
  
  gcm_emit_method(&buffer,NV3062TCL_SET_COLOR_FORMAT,2);
  gcm_emit(&buffer,0x0b);
  gcm_emit(&buffer,0x10001000);
  
  gcm_emit_method(&buffer,NV308ATCL_POINT,3);
  gcm_emit(&buffer,shift);
  gcm_emit(&buffer,(1 << 16) | width);
  gcm_emit(&buffer,(1 << 16) | width);
  
  gcm_emit(&buffer,NV308ATCL_COLOR | (width_pad << 18));
  
  size_t i_word = 0;
  for(;i_word < width;++i_word) {
    ieee32_t tmp;
    tmp.h.a[0] = pvalues[i_word].h.a[1];
    tmp.h.a[1] = pvalues[i_word].h.a[0];
    
    gcm_emit(&buffer,tmp.u);
  }
  for(;i_word < width_pad;++i_word) {
    gcm_emit(&buffer,0);
  }
  
  gcm_finish_commands(context,&buffer);
}

void
rsxgl_uniforms_validate(rsxgl_context_t * ctx,program_t & program)
{
  if(program.invalid_uniforms) {
    gcmContextData * context = ctx -> base.gcm_context;

    //rsxgl_debug_printf("invalid uniforms:\n");
    
    program_t::uniform_table_type::value_type * puniform = program.uniform_table_values;
    //const char * names = program.names_data;

    const ieee32_t * values = program.uniform_values;

    program_t::uniform_size_type n_validated_fp_uniforms = 0;

    for(program_t::uniform_size_type i = 0,n = program.uniform_table_size;i < n;++i,++puniform) {
      program_t::uniform_t & uniform = puniform -> second;

      program_t::uniform_size_type width = 0;
      switch(uniform.type) {
      case RSXGL_DATA_TYPE_FLOAT:
	width = 1;
	break;
      case RSXGL_DATA_TYPE_FLOAT2:
	width = 2;
	break;
      case RSXGL_DATA_TYPE_FLOAT3:
	width = 3;
	break;
      case RSXGL_DATA_TYPE_FLOAT4:
	width = 4;
	break;
      case RSXGL_DATA_TYPE_FLOAT4x4:
	width = 4;
	break;
      case RSXGL_DATA_TYPE_SAMPLER1D:
      case RSXGL_DATA_TYPE_SAMPLER2D:
      case RSXGL_DATA_TYPE_SAMPLER3D:
      case RSXGL_DATA_TYPE_SAMPLERCUBE:
      case RSXGL_DATA_TYPE_SAMPLERRECT:
	width = 1;
      default:
	width = 0;
	break;
      }

      const program_t::uniform_size_type count = uniform.count;

      if(uniform.invalid.any()) {
	//rsxgl_debug_printf("\t%u invalid\n",i);

	if(uniform.invalid.test(RSXGL_VERTEX_SHADER)) {
	  const ieee32_t * pvalues = values + uniform.values_index;

	  program_t::uniform_size_type index = uniform.vp_index;
	  uint32_t * buffer = gcm_reserve(context,6 * count);
	    
	  for(program_t::uniform_size_type j = 0;j < count;++j,++index,pvalues += width) {
	    //rsxgl_debug_printf("%u/%u %u: %u: %f %f %f %f\n",
	    //	 j,count,width,
	    //	 pvalues - values,
	    //	 pvalues[0].f,pvalues[1].f,pvalues[2].f,pvalues[3].f);
	    
	    gcm_emit_method(&buffer,NV30_3D_VP_UPLOAD_CONST_ID,5);
	    gcm_emit(&buffer,index);
	    
	    gcm_emit(&buffer,width > 0 ? pvalues[0].u : 0);
	    gcm_emit(&buffer,width > 1 ? pvalues[1].u : 0);
	    gcm_emit(&buffer,width > 2 ? pvalues[2].u : 0);
	    gcm_emit(&buffer,width > 3 ? pvalues[3].u : 0);
	  }
	  
	  gcm_finish_commands(context,&buffer);
	}
	
	if(uniform.invalid.test(RSXGL_FRAGMENT_SHADER)) {
	  //rsxgl_debug_printf("fp ");

	  const ieee32_t * pvalues = values + uniform.values_index;
	  const program_t::instruction_size_type * pfp_offsets = program.program_offsets + uniform.program_offsets_index;

	  for(program_t::uniform_size_type j = 0;j < count;++j,pvalues += width) {
	    for(program_t::instruction_size_type offsets_count = *pfp_offsets++;offsets_count > 0;--offsets_count,++pfp_offsets) {
	      rsxgl_inline_transfer(context,rsxgl_rsx_ucode_offset(program.fp_ucode_offset + *pfp_offsets++),width,pvalues);
	    }
	  }

	  ++n_validated_fp_uniforms;
	}

	//rsxgl_debug_printf("\n");

	uniform.invalid.reset();
      }
    }

    if(n_validated_fp_uniforms > 0) {
      uint32_t * buffer = gcm_reserve(context,2);

      gcm_emit_method(&buffer,NV30_3D_FP_ACTIVE_PROGRAM,1);
      gcm_emit(&buffer,rsxgl_rsx_ucode_offset(program.fp_ucode_offset) | NV30_3D_FP_ACTIVE_PROGRAM_DMA0);

      gcm_finish_commands(context,&buffer);
    }

    program.invalid_uniforms = 0;
  }
}
