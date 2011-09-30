// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// textures.cc - Handle texture maps.

#include "debug.h"
#include "rsxgl_assert.h"
#include "rsxgl_context.h"
#include "gl_constants.h"
#include "textures.h"

#include <GL3/gl3.h>
#include "GL3/gl3ext.h"
#include "error.h"

#include "gcm.h"
#include "nv40.h"
#include "gl_fifo.h"
#include "cxxutil.h"
#include "timestamp.h"

#include <malloc.h>
#include <string.h>

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

// Samplers:
sampler_t::storage_type & sampler_t::storage()
{
  static sampler_t::storage_type _storage(0,0);
  return _storage;
}

sampler_t::sampler_t()
{
  wrap_s = RSXGL_REPEAT;
  wrap_t = RSXGL_REPEAT;
  wrap_r = RSXGL_REPEAT;

  filter_min = RSXGL_NEAREST;
  filter_mag = RSXGL_NEAREST;

  compare_mode = 0;
  compare_func = RSXGL_NEVER;

  lodBias = 0.0f;
  minLod = 0.0f;
  maxLod = 12.0f;
}

GLAPI void APIENTRY
glGenSamplers (GLsizei count, GLuint *samplers)
{
  GLsizei n = sampler_t::storage().create_names(count,samplers);

  if(count != n) {
    RSXGL_ERROR_(GL_OUT_OF_MEMORY);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDeleteSamplers (GLsizei count, const GLuint *samplers)
{
  struct rsxgl_context_t * ctx = current_ctx();

  for(GLsizei i = 0;i < count;++i,++samplers) {
    GLuint sampler_name = *samplers;

    if(sampler_name == 0) continue;

    // Free resources used by this object:
    if(sampler_t::storage().is_object(sampler_name)) {
      const sampler_t & sampler = sampler_t::storage().at(sampler_name);
      
      // If this sampler_name is bound to any sampler_name targets, unbind it from them:
      const sampler_t::binding_bitfield_type binding = sampler.binding_bitfield;
      if(binding.any()) {
	for(size_t rsx_target = 0;rsx_target < RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;++rsx_target) {
	  if(binding[rsx_target]) {
	    ctx -> sampler_binding.bind(rsx_target,0);
	  }
	}
      }
    }

    // Delete the name:
    if(sampler_t::storage().is_name(sampler_name)) {
      sampler_t::storage().destroy(sampler_name);
    }
  }

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsSampler (GLuint sampler)
{
  return sampler_t::storage().is_object(sampler);
}

GLAPI void APIENTRY
glBindSampler (GLuint unit, GLuint sampler_name)
{
  if(unit > RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!(sampler_name == 0 || sampler_t::storage().is_name(sampler_name))) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(sampler_name != 0 && !sampler_t::storage().is_object(sampler_name)) {
    sampler_t::storage().create_object(sampler_name);
  }

  struct rsxgl_context_t * ctx = current_ctx();
  GLuint prev_sampler_name = ctx -> sampler_binding.names[unit];
  ctx -> sampler_binding.bind(unit,sampler_name);
  ctx -> invalid_samplers.set(unit);

  if(prev_sampler_name == 0 && sampler_name != 0 && ctx -> texture_binding.names[unit] != 0) {
    texture_t::storage().at(ctx -> texture_binding.names[unit]).sampler.binding_bitfield.reset(ctx -> active_texture);
  }
}

// This function could potentially be called by either glSamplerParameter or glTexParameter:
static inline void
_rsxgl_set_sampler_parameteri(rsxgl_context_t * ctx,sampler_t & sampler,GLenum pname, uint32_t param)
{
  if(pname == GL_TEXTURE_MIN_FILTER) {
    switch(param) {
    case GL_NEAREST:
      sampler.filter_min = RSXGL_NEAREST;
      break;
    case GL_LINEAR:
      sampler.filter_min = RSXGL_LINEAR;
      break;
    case GL_NEAREST_MIPMAP_NEAREST:
      sampler.filter_min = RSXGL_NEAREST_MIPMAP_NEAREST;
      break;
    case GL_LINEAR_MIPMAP_NEAREST:
      sampler.filter_min = RSXGL_LINEAR_MIPMAP_NEAREST;
      break;
    case GL_NEAREST_MIPMAP_LINEAR:
      sampler.filter_min = RSXGL_LINEAR_MIPMAP_LINEAR;
      break;
    case GL_LINEAR_MIPMAP_LINEAR:
      sampler.filter_min = RSXGL_NEAREST_MIPMAP_LINEAR;
      break;
    };
  }
  else if(pname == GL_TEXTURE_MAG_FILTER) {
    switch(param) {
    case GL_NEAREST:
      sampler.filter_mag = RSXGL_NEAREST;
      break;
    case GL_LINEAR:
      sampler.filter_mag = RSXGL_LINEAR;
      break;
    default:
      break;
    };
  }
  else if(pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R) {
    uint8_t value = 0;
    switch(param) {
    case GL_REPEAT:
      value = RSXGL_REPEAT;
      break;
    case GL_CLAMP_TO_EDGE:
      value = RSXGL_CLAMP_TO_EDGE;
      break;
    case GL_MIRRORED_REPEAT:
      value = RSXGL_MIRRORED_REPEAT;
      break;
    default:
      break;
    };

    switch(pname) {
    case GL_TEXTURE_WRAP_S:
      sampler.wrap_s = value;
      break;
    case GL_TEXTURE_WRAP_T:
      sampler.wrap_t = value;
      break;
    case GL_TEXTURE_WRAP_R:
      sampler.wrap_r = value;
      break;
    default:
      break;
    };
  }
  else if(pname == GL_TEXTURE_COMPARE_MODE) {
    switch(param) {
    case GL_COMPARE_REF_TO_TEXTURE:
      sampler.compare_mode = 1;
      break;
    case GL_NONE:
      sampler.compare_mode = 0;
    default:
      break;
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_FUNC) {
    switch(param) {
    case GL_NEVER:
      sampler.compare_func = RSXGL_TEXTURE_COMPARE_NEVER;
      break;
    case GL_LESS:
      sampler.compare_func = RSXGL_TEXTURE_COMPARE_LESS;
      break;
    case GL_EQUAL:
      sampler.compare_func = RSXGL_TEXTURE_COMPARE_EQUAL;
      break;
    case GL_LEQUAL:
      sampler.compare_func = RSXGL_TEXTURE_COMPARE_LEQUAL;
      break;
    case GL_GREATER:
      sampler.compare_func = RSXGL_TEXTURE_COMPARE_GREATER;
      break;
    case GL_NOTEQUAL:
      sampler.compare_func = RSXGL_TEXTURE_COMPARE_NOTEQUAL;
      break;
    case GL_GEQUAL:
      sampler.compare_func = RSXGL_TEXTURE_COMPARE_GEQUAL;
      break;
    case GL_ALWAYS:
      sampler.compare_func = RSXGL_TEXTURE_COMPARE_ALWAYS;
      break;
    default:
      break;
    }
  }
}

static inline void
_rsxgl_get_sampler_parameteri(rsxgl_context_t * ctx,sampler_t & sampler,GLenum pname, uint32_t * param)
{
  if(pname == GL_TEXTURE_MIN_FILTER) {
    switch(sampler.filter_min) {
    case RSXGL_NEAREST:
      *param = GL_NEAREST;
      break;
    case RSXGL_LINEAR:
      *param = GL_LINEAR;
      break;
    case RSXGL_NEAREST_MIPMAP_NEAREST:
      *param = GL_NEAREST_MIPMAP_NEAREST;
      break;
    case RSXGL_LINEAR_MIPMAP_NEAREST:
      *param = GL_LINEAR_MIPMAP_NEAREST;
      break;
    case RSXGL_LINEAR_MIPMAP_LINEAR:
      *param = GL_NEAREST_MIPMAP_LINEAR;
      break;
    case RSXGL_NEAREST_MIPMAP_LINEAR:
      *param = GL_LINEAR_MIPMAP_LINEAR;
      break;
    };
  }
  else if(pname == GL_TEXTURE_MAG_FILTER) {
    switch(sampler.filter_mag) {
    case RSXGL_NEAREST:
      *param = GL_NEAREST;
      break;
    case RSXGL_LINEAR:
      *param = GL_LINEAR;
      break;
    default:
      break;
    };
  }
  else if(pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R) {
    uint8_t value = 0;

    switch(pname) {
    case GL_TEXTURE_WRAP_S:
      value = sampler.wrap_s;
      break;
    case GL_TEXTURE_WRAP_T:
      value = sampler.wrap_t;
      break;
    case GL_TEXTURE_WRAP_R:
      value = sampler.wrap_r;
      break;
    default:
      break;
    };

    switch(value) {
    case RSXGL_REPEAT:
      *param = GL_REPEAT;
      break;
    case RSXGL_CLAMP_TO_EDGE:
      *param = GL_CLAMP_TO_EDGE;
      break;
    case RSXGL_MIRRORED_REPEAT:
      *param = GL_MIRRORED_REPEAT;
      break;
    default:
      break;
    };
  }
  else if(pname == GL_TEXTURE_COMPARE_MODE) {
    switch(sampler.compare_mode) {
    case 1:
      *param = GL_COMPARE_REF_TO_TEXTURE;
      break;
    case 0:
      *param = GL_NONE;
      break;
    default:
      break;
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_FUNC) {
    switch(sampler.compare_func) {
    case RSXGL_TEXTURE_COMPARE_NEVER:
      *param = GL_NEVER;
      break;
    case RSXGL_TEXTURE_COMPARE_LESS:
      *param = GL_LESS;
      break;
    case RSXGL_TEXTURE_COMPARE_EQUAL:
      *param = GL_EQUAL;
      break;
    case RSXGL_TEXTURE_COMPARE_LEQUAL:
      *param = GL_LEQUAL;
      break;
    case RSXGL_TEXTURE_COMPARE_GREATER:
      *param = GL_GREATER;
      break;
    case RSXGL_TEXTURE_COMPARE_NOTEQUAL:
      *param = GL_NOTEQUAL;
      break;
    case RSXGL_TEXTURE_COMPARE_GEQUAL:
      *param = GL_GEQUAL;
      break;
    case RSXGL_TEXTURE_COMPARE_ALWAYS:
      *param = GL_ALWAYS;
      break;
    default:
      break;
    }
  }
}

static inline void
_rsxgl_set_sampler_parameterf(rsxgl_context_t * ctx,sampler_t & sampler,GLenum pname,float param)
{
  if(pname == GL_TEXTURE_MIN_LOD) {
    sampler.minLod = param;
  }
  else if(pname == GL_TEXTURE_MAX_LOD) {
    sampler.maxLod = param;
  }
  else if(pname == GL_TEXTURE_LOD_BIAS) {
    sampler.lodBias = param;
  }
  else {
    _rsxgl_set_sampler_parameteri(ctx,sampler,pname,param);
  }
}

static inline void
_rsxgl_get_sampler_parameterf(rsxgl_context_t * ctx,sampler_t & sampler,GLenum pname,float * param)
{
  if(pname == GL_TEXTURE_MIN_LOD) {
    *param = sampler.minLod;
  }
  else if(pname == GL_TEXTURE_MAX_LOD) {
    *param = sampler.maxLod;
  }
  else if(pname == GL_TEXTURE_LOD_BIAS) {
    *param = sampler.lodBias;
  }
  else {
    uint32_t _param = 0;
    _rsxgl_get_sampler_parameteri(ctx,sampler,pname,&_param);
    *param = _param;
  }
}

static inline void
rsxgl_sampler_parameteri(rsxgl_context_t * ctx,sampler_t::name_type sampler_name,GLenum pname,uint32_t param)
{
  if(pname == GL_TEXTURE_MIN_FILTER) {
    if(!(param == GL_NEAREST || param == GL_LINEAR || param == GL_NEAREST_MIPMAP_NEAREST || param == GL_LINEAR_MIPMAP_NEAREST || param == GL_NEAREST_MIPMAP_LINEAR || param == GL_LINEAR_MIPMAP_LINEAR)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_MAG_FILTER) {
    if(!(param == GL_NEAREST || param == GL_LINEAR)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_T) {
    if(!(param == GL_CLAMP_TO_EDGE || param == GL_MIRRORED_REPEAT || param == GL_REPEAT)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_MODE) {
    if(!(param == GL_COMPARE_REF_TO_TEXTURE || pname == GL_NONE)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_FUNC) {
    if(!(param == GL_NEVER || param == GL_LESS || param == GL_EQUAL || param == GL_LEQUAL || param == GL_GREATER || param == GL_NOTEQUAL || param == GL_GEQUAL || param == GL_ALWAYS)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!sampler_t::storage().is_object(sampler_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  sampler_t & sampler = sampler_t::storage().at(sampler_name);
  _rsxgl_set_sampler_parameteri(ctx,sampler,pname,param);
  ctx -> invalid_samplers |= sampler.binding_bitfield;

  RSXGL_NOERROR_();
}

static inline void
rsxgl_get_sampler_parameteri(rsxgl_context_t * ctx,sampler_t::name_type sampler_name,GLenum pname,uint32_t * param)
{
  if(!(pname == GL_TEXTURE_MIN_FILTER ||
       pname == GL_TEXTURE_MAG_FILTER ||
       pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_T ||
       pname == GL_TEXTURE_COMPARE_MODE ||
       pname == GL_TEXTURE_COMPARE_FUNC)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!sampler_t::storage().is_object(sampler_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  sampler_t & sampler = sampler_t::storage().at(sampler_name);
  _rsxgl_get_sampler_parameteri(ctx,sampler,pname,param);

  RSXGL_NOERROR_();
}

static inline void
rsxgl_sampler_parameterf(rsxgl_context_t * ctx,sampler_t::name_type sampler_name,GLenum pname,float param)
{
  if(pname == GL_TEXTURE_MIN_FILTER) {
    if(!(param == GL_NEAREST || param == GL_LINEAR || param == GL_NEAREST_MIPMAP_NEAREST || param == GL_LINEAR_MIPMAP_NEAREST || param == GL_NEAREST_MIPMAP_LINEAR || param == GL_LINEAR_MIPMAP_LINEAR)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_MAG_FILTER) {
    if(!(param == GL_NEAREST || param == GL_LINEAR)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_T) {
    if(!(param == GL_CLAMP_TO_EDGE || param == GL_MIRRORED_REPEAT || param == GL_REPEAT)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_MODE) {
    if(!(param == GL_COMPARE_REF_TO_TEXTURE || pname == GL_NONE)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_FUNC) {
    if(!(param == GL_NEVER || param == GL_LESS || param == GL_EQUAL || param == GL_LEQUAL || param == GL_GREATER || param == GL_NOTEQUAL || param == GL_GEQUAL || param == GL_ALWAYS)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(!(pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD || pname == GL_TEXTURE_LOD_BIAS)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!sampler_t::storage().is_object(sampler_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  sampler_t & sampler = sampler_t::storage().at(sampler_name);
  if(pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD || pname == GL_TEXTURE_LOD_BIAS) {
    _rsxgl_set_sampler_parameterf(ctx,sampler,pname,param);
  }
  else {
    _rsxgl_set_sampler_parameteri(ctx,sampler,pname,param);
  }
  ctx -> invalid_samplers |= sampler.binding_bitfield;

  RSXGL_NOERROR_();
}

static inline void
rsxgl_get_sampler_parameterf(rsxgl_context_t * ctx,sampler_t::name_type sampler_name,GLenum pname,float * param)
{
  if(!(pname == GL_TEXTURE_MIN_FILTER ||
       pname == GL_TEXTURE_MAG_FILTER ||
       pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_T ||
       pname == GL_TEXTURE_COMPARE_MODE ||
       pname == GL_TEXTURE_COMPARE_FUNC ||
       pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD || pname == GL_TEXTURE_LOD_BIAS)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!sampler_t::storage().is_object(sampler_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  sampler_t & sampler = sampler_t::storage().at(sampler_name);
  if(pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD || pname == GL_TEXTURE_LOD_BIAS) {
    _rsxgl_get_sampler_parameterf(ctx,sampler,pname,param);
  }
  else {
    uint32_t value = 0;
    _rsxgl_get_sampler_parameteri(ctx,sampler,pname,&value);
    *param = value;
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glSamplerParameteri (GLuint sampler_name, GLenum pname, GLint param)
{
  rsxgl_sampler_parameteri(current_ctx(),sampler_name,pname,param);
}

GLAPI void APIENTRY
glSamplerParameteriv (GLuint sampler_name, GLenum pname, const GLint *param)
{
  rsxgl_sampler_parameteri(current_ctx(),sampler_name,pname,*param);
}

GLAPI void APIENTRY
glSamplerParameterf (GLuint sampler_name, GLenum pname, GLfloat param)
{
  rsxgl_sampler_parameterf(current_ctx(),sampler_name,pname,param);
}

GLAPI void APIENTRY
glSamplerParameterfv (GLuint sampler_name, GLenum pname, const GLfloat *param)
{
  rsxgl_sampler_parameterf(current_ctx(),sampler_name,pname,*param);
}

GLAPI void APIENTRY
glGetSamplerParameteriv (GLuint sampler_name, GLenum pname, GLint *params)
{
  uint32_t value = 0;
  rsxgl_get_sampler_parameteri(current_ctx(),sampler_name,pname,&value);
  *params = value;
}

GLAPI void APIENTRY
glGetSamplerParameterfv (GLuint sampler_name, GLenum pname, GLfloat *params)
{
  float value = 0;
  rsxgl_get_sampler_parameterf(current_ctx(),sampler_name,pname,&value);
  *params = value;
}

// Textures:
texture_t::storage_type & texture_t::storage()
{
  static texture_t::storage_type _storage(0,0);
  return _storage;
}

texture_t::texture_t()
  : deleted(0), timestamp(0), invalid_storage(0), valid(0), immutable(0), internalformat(RSXGL_TEX_FORMAT_INVALID), cube(0), rect(0), max_level(0), dims(0), format(0), pitch(0), remap(0)
{
  size[0] = 0;
  size[1] = 0;
  size[2] = 0;
}

void
texture_t::destroy()
{
}

texture_t::level_t::level_t()
  : invalid_contents(0), internalformat(RSXGL_TEX_FORMAT_INVALID), dims(0), data(0)
{
  size[0] = 0;
  size[1] = 0;
  size[2] = 0;
}

texture_t::level_t::~level_t()
{
  if(data != 0) free(data);
}

GLAPI void APIENTRY
glGenTextures (GLsizei n, GLuint *textures)
{
  GLsizei count = texture_t::storage().create_names(n,textures);

  if(count != n) {
    RSXGL_ERROR_(GL_OUT_OF_MEMORY);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDeleteTextures (GLsizei n, const GLuint *textures)
{
  struct rsxgl_context_t * ctx = current_ctx();

  for(GLsizei i = 0;i < n;++i,++textures) {
    GLuint texture_name = *textures;

    if(texture_name == 0) continue;

    // Free resources used by this object:
    if(texture_t::storage().is_object(texture_name)) {
      const texture_t & texture = texture_t::storage().at(texture_name);
      
      // If this texture_name is bound to any texture_name targets, unbind it from them:
      const texture_t::binding_bitfield_type binding = texture.binding_bitfield;
      if(binding.any()) {
	for(size_t rsx_target = 0;rsx_target < RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;++rsx_target) {
	  if(binding[rsx_target]) {
	    ctx -> texture_binding.bind(rsx_target,0);
	  }
	}
      }

      // Free memory used by this texture:
      if(texture.memory.offset != 0) {
	rsxgl_arena_free(memory_arena_t::storage().at(texture.arena),texture.memory);
      }
    }

    // Delete the name:
    if(texture_t::storage().is_name(texture_name)) {
      texture_t::storage().destroy(texture_name);
    }
  }

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsTexture (GLuint texture)
{
  return texture_t::storage().is_object(texture);
}

GLAPI void APIENTRY
glActiveTexture (GLenum texture)
{
  texture_t::binding_type::size_type unit = (uint32_t)texture - (uint32_t)GL_TEXTURE0;
  if(unit > RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  struct rsxgl_context_t * ctx = current_ctx();

  ctx -> active_texture = unit;
}

GLAPI void APIENTRY
glBindTexture (GLenum target, GLuint texture_name)
{
  if(!(target == GL_TEXTURE_1D ||
       target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_3D ||
       target == GL_TEXTURE_1D_ARRAY ||
       target == GL_TEXTURE_2D_ARRAY ||
       target == GL_TEXTURE_RECTANGLE ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_2D_MULTISAMPLE || 
       target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!(texture_name == 0 || texture_t::storage().is_name(texture_name))) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(texture_name != 0 && !texture_t::storage().is_object(texture_name)) {
    texture_t::storage().create_object(texture_name);
  }

  rsxgl_context_t * ctx = current_ctx();
  ctx -> texture_binding.bind(ctx -> active_texture,texture_name);
  ctx -> invalid_textures.set(ctx -> active_texture);

  if(texture_name != 0 && ctx -> sampler_binding.names[ctx -> active_texture] == 0) {
    texture_t::storage().at(texture_name).sampler.binding_bitfield.set(ctx -> active_texture);
    ctx -> invalid_samplers.set(ctx -> active_texture);
  }

  RSXGL_NOERROR_();
}

static inline void
rsxgl_tex_parameteri(rsxgl_context_t * ctx,texture_t::name_type texture_name,GLenum pname,uint32_t param)
{
  if(pname == GL_TEXTURE_MIN_FILTER) {
    if(!(param == GL_NEAREST || param == GL_LINEAR || param == GL_NEAREST_MIPMAP_NEAREST || param == GL_LINEAR_MIPMAP_NEAREST || param == GL_NEAREST_MIPMAP_LINEAR || param == GL_LINEAR_MIPMAP_LINEAR)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_MAG_FILTER) {
    if(!(param == GL_NEAREST || param == GL_LINEAR)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_T) {
    if(!(param == GL_CLAMP_TO_EDGE || param == GL_MIRRORED_REPEAT || param == GL_REPEAT)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_MODE) {
    if(!(param == GL_COMPARE_REF_TO_TEXTURE || pname == GL_NONE)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_FUNC) {
    if(!(param == GL_NEVER || param == GL_LESS || param == GL_EQUAL || param == GL_LEQUAL || param == GL_GREATER || param == GL_NOTEQUAL || param == GL_GEQUAL || param == GL_ALWAYS)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_SWIZZLE_R || pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A) {
    if(!(param == GL_RED || param == GL_GREEN || param == GL_BLUE || param == GL_ALPHA || param == GL_ZERO || param == GL_ONE)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!texture_t::storage().is_object(texture_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  texture_t & texture = texture_t::storage().at(texture_name);

  if(pname == GL_TEXTURE_SWIZZLE_R || pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A) {
    ctx -> invalid_textures |= texture.binding_bitfield;
  }
  else {
    _rsxgl_set_sampler_parameteri(ctx,texture.sampler,pname,param);
    ctx -> invalid_samplers |= texture.sampler.binding_bitfield;
  }

  RSXGL_NOERROR_();
}

static inline void
rsxgl_get_texture_parameteri(rsxgl_context_t * ctx,texture_t::name_type texture_name,GLenum pname,uint32_t * param)
{
  if(!(pname == GL_TEXTURE_MIN_FILTER ||
       pname == GL_TEXTURE_MAG_FILTER ||
       pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_T ||
       pname == GL_TEXTURE_COMPARE_MODE ||
       pname == GL_TEXTURE_COMPARE_FUNC)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!texture_t::storage().is_object(texture_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  texture_t & texture = texture_t::storage().at(texture_name);
  if(pname == GL_TEXTURE_SWIZZLE_R || pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A) {
  }
  else {  
    _rsxgl_get_sampler_parameteri(ctx,texture.sampler,pname,param);
  }

  RSXGL_NOERROR_();
}

static inline void
rsxgl_tex_parameterf(rsxgl_context_t * ctx,texture_t::name_type texture_name,GLenum pname,float param)
{
  if(pname == GL_TEXTURE_MIN_FILTER) {
    if(!(param == GL_NEAREST || param == GL_LINEAR || param == GL_NEAREST_MIPMAP_NEAREST || param == GL_LINEAR_MIPMAP_NEAREST || param == GL_NEAREST_MIPMAP_LINEAR || param == GL_LINEAR_MIPMAP_LINEAR)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_MAG_FILTER) {
    if(!(param == GL_NEAREST || param == GL_LINEAR)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_T) {
    if(!(param == GL_CLAMP_TO_EDGE || param == GL_MIRRORED_REPEAT || param == GL_REPEAT)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_MODE) {
    if(!(param == GL_COMPARE_REF_TO_TEXTURE || pname == GL_NONE)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_COMPARE_FUNC) {
    if(!(param == GL_NEVER || param == GL_LESS || param == GL_EQUAL || param == GL_LEQUAL || param == GL_GREATER || param == GL_NOTEQUAL || param == GL_GEQUAL || param == GL_ALWAYS)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(pname == GL_TEXTURE_SWIZZLE_R || pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A) {
    if(!(param == GL_RED || param == GL_GREEN || param == GL_BLUE || param == GL_ALPHA || param == GL_ZERO || param == GL_ONE)) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(!(pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD || pname == GL_TEXTURE_LOD_BIAS)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!texture_t::storage().is_object(texture_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  texture_t & texture = texture_t::storage().at(texture_name);
  
  if(pname == GL_TEXTURE_SWIZZLE_R || pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A) {
    uint32_t remap = texture.remap;
    
    uint32_t op_shift = 
      (pname == GL_TEXTURE_SWIZZLE_R ? NV30_3D_TEX_SWIZZLE_S0_Z__SHIFT :
       pname == GL_TEXTURE_SWIZZLE_G ? NV30_3D_TEX_SWIZZLE_S0_Y__SHIFT :
       pname == GL_TEXTURE_SWIZZLE_B ? NV30_3D_TEX_SWIZZLE_S0_X__SHIFT :
       NV30_3D_TEX_SWIZZLE_S0_W__SHIFT);
    uint32_t element_shift = 
      (pname == GL_TEXTURE_SWIZZLE_R ? NV30_3D_TEX_SWIZZLE_S1_Z__SHIFT :
       pname == GL_TEXTURE_SWIZZLE_G ? NV30_3D_TEX_SWIZZLE_S1_Y__SHIFT :
       pname == GL_TEXTURE_SWIZZLE_B ? NV30_3D_TEX_SWIZZLE_S1_X__SHIFT :
       NV30_3D_TEX_SWIZZLE_S1_W__SHIFT);
    remap &= ~((uint32_t)0x3 << op_shift) | ((uint32_t)0x3 << element_shift);
    remap |= 
      ((param == GL_ZERO ? RSXGL_TEXTURE_REMAP_ZERO : param == GL_ONE ? RSXGL_TEXTURE_REMAP_ONE : RSXGL_TEXTURE_REMAP_REMAP) << op_shift) |
      ((param == GL_RED ? 2 : param == GL_GREEN ? 1 : param == GL_BLUE ? 0 : 3) << element_shift);
    
    texture.remap = remap;
    
    ctx -> invalid_textures |= texture.binding_bitfield;
  }
  else {
    _rsxgl_set_sampler_parameterf(ctx,texture.sampler,pname,param);
    ctx -> invalid_samplers |= texture.sampler.binding_bitfield;
  }

  RSXGL_NOERROR_();
}

static inline void
rsxgl_get_texture_parameterf(rsxgl_context_t * ctx,texture_t::name_type texture_name,GLenum pname,float * param)
{
  if(!(pname == GL_TEXTURE_MIN_FILTER ||
       pname == GL_TEXTURE_MAG_FILTER ||
       pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_T ||
       pname == GL_TEXTURE_COMPARE_MODE ||
       pname == GL_TEXTURE_COMPARE_FUNC ||
       pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD || pname == GL_TEXTURE_LOD_BIAS ||
       pname == GL_TEXTURE_SWIZZLE_R || pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!texture_t::storage().is_object(texture_name)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  texture_t & texture = texture_t::storage().at(texture_name);
  if(pname == GL_TEXTURE_SWIZZLE_R || pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A) {
    
  }
  else {
    _rsxgl_get_sampler_parameterf(ctx,texture.sampler,pname,param);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
  if(!(target == GL_TEXTURE_1D ||
       target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_3D ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_RECTANGLE)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  rsxgl_tex_parameterf(ctx,ctx -> texture_binding.names[0],pname,param);
}

GLAPI void APIENTRY
glTexParameterfv (GLenum target, GLenum pname, const GLfloat *params)
{
  if(!(target == GL_TEXTURE_1D ||
       target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_3D ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_RECTANGLE)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  rsxgl_tex_parameterf(ctx,ctx -> texture_binding.names[0],pname,*params);
}

GLAPI void APIENTRY
glTexParameteri (GLenum target, GLenum pname, GLint param)
{
  if(!(target == GL_TEXTURE_1D ||
       target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_3D ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_RECTANGLE)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  rsxgl_tex_parameteri(ctx,ctx -> texture_binding.names[ctx -> active_texture],pname,param);
}

GLAPI void APIENTRY
glTexParameteriv (GLenum target, GLenum pname, const GLint *params)
{
  if(!(target == GL_TEXTURE_1D ||
       target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_3D ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_RECTANGLE)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  rsxgl_tex_parameteri(ctx,ctx -> texture_binding.names[ctx -> active_texture],pname,*params);
}

static inline uint8_t
rsxgl_tex_format(GLint internalformat)
{
  if(internalformat == GL_RGBA || internalformat == GL_RGBA8) {
    return RSXGL_TEX_FORMAT_A8R8G8B8;
  }
  else if(internalformat == GL_RGB || internalformat == GL_RGB8) {
    return RSXGL_TEX_FORMAT_D8R8G8B8;
  }
  else if(internalformat == GL_RED || internalformat == GL_R8) {
    return RSXGL_TEX_FORMAT_B8;
  }
  else if(internalformat == GL_RG || internalformat == GL_RG8) {
    return RSXGL_TEX_FORMAT_G8B8;
  }
  else if(internalformat == GL_DEPTH_COMPONENT16 || internalformat == GL_DEPTH_COMPONENT) {
    return RSXGL_TEX_FORMAT_DEPTH16;
  }
  else if(internalformat == GL_DEPTH_COMPONENT24) {
    return RSXGL_TEX_FORMAT_DEPTH24_D8;
  }
  else if(internalformat == GL_DEPTH_COMPONENT32F) {
    return RSXGL_TEX_FORMAT_DEPTH24_D8_FLOAT;
  }
  else {
    return RSXGL_TEX_FORMAT_INVALID;
  }
}

static const uint8_t rsx_texture_formats[] = {
  0x81,
  0x82,
  0x83,
  0x84,
  0x85,
  0x86,
  0x87,
  0x88,
  0x8B,
  0x8F,
  0x90,
  0x91,
  0x92,
  0x93,
  0x94,
  0x95,
  0x97,
  0x98,
  0x99,
  0x9A,
  0x9B,
  0x9C,
  0x9D,
  0x9E,
  0x9F,
  0xAD,
  0xAE
};

static const uint8_t
rsxgl_tex_bytesPerPixel[] = {
  1,
  2,
  2,
  2,
  4,
  0,
  0,
  0,
  2,
  2,
  4,
  4,
  2,
  2,
  2,
  4,
  2,
  0,
  0,
  0,
  8,
  16,
  4,
  2,
  4,
  4,
  0,
  0
};

static inline uint32_t
rsxgl_tex_remap(uint32_t op0,uint32_t op1,uint32_t op2,uint32_t op3,
		uint32_t src0,uint32_t src1,uint32_t src2,uint32_t src3)
{
  return
    (op0 << 14) |
    (op1 << 12) |
    (op2 << 10) |
    (op3 << 8) |
    (src0 << 6) |
    (src1 << 4) |
    (src2 << 2) |
    (src3 << 0);
}

static inline void
rsxgl_tex_copy_image(void * dst,uint32_t dstRowPitch,uint32_t dstX,uint32_t dstY,uint32_t dstZ,
		     const void * src,uint32_t srcRowPitch,uint32_t srcX,uint32_t srcY,uint32_t srcZ,
		     uint32_t width,uint32_t height,uint32_t depth,uint32_t bytesPerPixel)
{
  uint8_t * pdst = (uint8_t *)dst + (dstZ * height * dstRowPitch) + (dstY * dstRowPitch) + (dstX * bytesPerPixel);
  const uint8_t * psrc = (const uint8_t *)src + (srcZ * height * srcRowPitch) + (srcY * srcRowPitch) + (srcX * bytesPerPixel);
  const uint32_t bytesPerLine = bytesPerPixel * width;

  for(size_t k = 0;k < depth;++k) {
    for(size_t j = 0;j < height;++j) {
      memcpy(pdst,psrc,bytesPerLine);
      pdst += dstRowPitch;
      psrc += srcRowPitch;
    }
  }
}

static inline void
rsxgl_tex_transfer_image(gcmContextData * context,
			 const memory_t & dst,uint32_t dstRowPitch,uint32_t dstX,uint32_t dstY,uint32_t dstZ,
			 const memory_t & src,uint32_t srcRowPitch,uint32_t srcX,uint32_t srcY,uint32_t srcZ,
			 uint32_t width,uint32_t height,uint32_t depth,uint32_t bytesPerPixel)
{
  const uint32_t linelength = width * bytesPerPixel;

  const uint32_t
    srcincr = height * srcRowPitch,
    dstincr = height * dstRowPitch;

  uint32_t
    srcoffset = src.offset + (srcZ * srcincr) + (srcY * srcRowPitch) + (srcX * bytesPerPixel),
    dstoffset = dst.offset + (dstZ * dstincr) + (dstY * dstRowPitch) + (dstX * bytesPerPixel);

  uint32_t * buffer = gcm_reserve(context,3 + (9 * depth));

  gcm_emit_channel_method(&buffer,1,0x184,2);
  gcm_emit(&buffer,(src.location == RSXGL_MEMORY_LOCATION_LOCAL) ? 0xFEED0000 : 0xFEED0001);
  gcm_emit(&buffer,(dst.location == RSXGL_MEMORY_LOCATION_LOCAL) ? 0xFEED0000 : 0xFEED0001);

  for(;depth > 0;--depth,srcoffset += srcincr,dstoffset += dstincr) {
    gcm_emit_channel_method(&buffer,1,0x30c,8);
    gcm_emit(&buffer,srcoffset);
    gcm_emit(&buffer,dstoffset);
    gcm_emit(&buffer,srcRowPitch);
    gcm_emit(&buffer,dstRowPitch);
    gcm_emit(&buffer,linelength);
    gcm_emit(&buffer,height);
    gcm_emit(&buffer,((uint32_t)1 << 8) | 1);
    gcm_emit(&buffer,0);
  }

  gcm_finish_commands(context,&buffer);
}

// given the format and type parameters passed to the likes of glTexImage*, return the "internal format"
// that can represent it:
static inline uint8_t
rsxgl_tex_internalformat(GLenum format,GLenum type)
{
  if(format == GL_RED) {
    if(type == GL_UNSIGNED_BYTE || type == GL_BYTE) {
      return RSXGL_TEX_FORMAT_B8;
    }
    else if(type == GL_UNSIGNED_SHORT || type == GL_SHORT) {
      return RSXGL_TEX_FORMAT_X16;
    }
    else if(type == GL_FLOAT) {
      return RSXGL_TEX_FORMAT_X32_FLOAT;
    }
  }
  else if(format == GL_RG) {
    if(type == GL_UNSIGNED_BYTE || type == GL_BYTE) {
      return RSXGL_TEX_FORMAT_G8B8;
    }
    else if(type == GL_UNSIGNED_SHORT || type == GL_SHORT) {
      return RSXGL_TEX_FORMAT_Y16_X16;
    }
    else if(type == GL_HALF_FLOAT) {
      return RSXGL_TEX_FORMAT_Y16_X16;
    }
  }
  else if(format == GL_BGRA) {
    if(type == GL_UNSIGNED_BYTE || type == GL_BYTE) {
      return RSXGL_TEX_FORMAT_D8R8G8B8;
    }
  }
}

static inline void
rsxgl_tex_set_image(uint8_t internalformat,uint32_t pitch,GLvoid * dstdata,
		    GLenum format,GLenum type,const GLvoid * srcdata,
		    uint32_t width,uint32_t height,uint32_t depth)
{
  uint32_t bytesPerPixel = 0;

  if(internalformat == RSXGL_TEX_FORMAT_B8) {
    if(format == GL_RED && (type == GL_UNSIGNED_BYTE || type == GL_BYTE)) {
      bytesPerPixel = sizeof(uint8_t);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_A1R5G5B5) {
    if(format == GL_BGRA && (type == GL_UNSIGNED_SHORT_5_5_5_1)) {
      bytesPerPixel = sizeof(uint16_t);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_A4R4G4B4) {
    if(format == GL_BGRA && (type == GL_UNSIGNED_SHORT_4_4_4_4)) {
      bytesPerPixel = sizeof(uint16_t);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_R5G6B5) {
    if(format == GL_BGR && (type == GL_UNSIGNED_SHORT_5_6_5)) {
      bytesPerPixel = sizeof(uint16_t);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_A8R8G8B8) {
    if(format == GL_BGRA && (type == GL_UNSIGNED_BYTE || type == GL_BYTE)) {
      bytesPerPixel = sizeof(uint8_t) * 4;
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_G8B8) {
    if(format == GL_RG && (type == GL_UNSIGNED_BYTE || type == GL_BYTE)) {
      bytesPerPixel = sizeof(uint8_t) * 2;
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_DEPTH24_D8) {
    if(format == GL_DEPTH_COMPONENT && (type == GL_UNSIGNED_INT || type == GL_INT)) {
      bytesPerPixel = sizeof(uint32_t);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_DEPTH24_D8_FLOAT) {
    if(format == GL_DEPTH_COMPONENT && (type == GL_FLOAT)) {
      bytesPerPixel = sizeof(float);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_DEPTH16) {
    if(format == GL_DEPTH_COMPONENT && (type == GL_UNSIGNED_SHORT || type == GL_SHORT)) {
      bytesPerPixel = sizeof(uint16_t);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_DEPTH16_FLOAT) {
    if(format == GL_DEPTH_COMPONENT && (type == GL_HALF_FLOAT)) {
      bytesPerPixel = sizeof(float) / 2;
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_X16) {
    if(format == GL_RED && (type == GL_UNSIGNED_SHORT || type == GL_SHORT)) {
      bytesPerPixel = sizeof(uint16_t);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_Y16_X16) {
    if(format == GL_RG && (type == GL_UNSIGNED_SHORT || type == GL_SHORT)) {
      bytesPerPixel = sizeof(uint16_t) * 2;
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_R5G5B5A1) {
    if(format == GL_BGRA && (type == GL_UNSIGNED_SHORT_1_5_5_5_REV)) {
      bytesPerPixel = sizeof(uint16_t);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_W16_Z16_Y16_X16_FLOAT) {
    if(format == GL_BGRA && (type == GL_HALF_FLOAT)) {
      bytesPerPixel = sizeof(float) / 2 * 4;
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_W32_Z32_Y32_X32_FLOAT) {
    if(format == GL_BGRA && (type == GL_FLOAT)) {
      bytesPerPixel = sizeof(float) * 4;
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_X32_FLOAT) {
    if(format == GL_RED && (type == GL_FLOAT)) {
      bytesPerPixel = sizeof(float);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_D1R5G5B5) {
    if((format == GL_BGRA || format == GL_BGR) && (type == GL_UNSIGNED_SHORT_5_6_5)) {
      bytesPerPixel = sizeof(uint16_t);
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_D8R8G8B8) {
    if(format == GL_BGRA && (type == GL_UNSIGNED_BYTE || type == GL_BYTE)) {
      bytesPerPixel = sizeof(uint8_t) * 4;
    }
  }
  else if(internalformat == RSXGL_TEX_FORMAT_Y16_X16_FLOAT) {
    if(format == GL_RG && (type == GL_HALF_FLOAT)) {
      bytesPerPixel = sizeof(float) / 2 * 2;
    }
  }

  // if we get here, then invoke an "efficient" transfer without type conversion or channel reordering:
  if(bytesPerPixel > 0) {
    rsxgl_tex_copy_image(dstdata,pitch,0,0,0,
			 srcdata,width * bytesPerPixel,0,0,0,
			 width,height,depth,bytesPerPixel);
  }
  else {
    rsxgl_debug_printf("texture image transfer could not be handled efficiently for: %u %x %x\n",internalformat,(uint32_t)format,(uint32_t)type);
  }
}

static inline uint32_t
rsxgl_tex_pixel_offset(const texture_t & texture,size_t level,uint32_t x,uint32_t y,uint32_t z,texture_t::dimension_size_type size[3])
{
  uint32_t offset = 0;
  const uint32_t bytesPerPixel = rsxgl_tex_bytesPerPixel[texture.internalformat];
  const uint32_t pitch = texture.pitch;

  size[0] = texture.size[0];
  size[1] = texture.size[1];
  size[2] = texture.size[2];

  for(size_t i = 0,n = std::min((size_t)texture.max_level,level);i < n;++i) {
    offset += pitch * size[1] * size[2];

    size[0] = std::max(size[0] >> 1,1);
    size[1] = std::max(size[1] >> 1,1);
    size[2] = std::max(size[2] >> 1,1);
  }

  return offset + (z * size[2] * pitch) + (y * pitch) + (x * bytesPerPixel);
}

static inline void
rsxgl_tex_image(rsxgl_context_t * ctx,texture_t & texture,const uint8_t dims,GLint level,uint8_t internalformat,GLsizei width,GLsizei height,GLsizei depth,
		GLenum format,GLenum type,const GLvoid * data)
{
  rsxgl_assert(dims > 0);
  rsxgl_assert(width > 0);
  rsxgl_assert(height > 0);
  rsxgl_assert(depth > 0);

  if(level < 0 || (boost::static_log2_argument_type)level >= texture_t::max_levels) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(internalformat == RSXGL_TEX_FORMAT_INVALID) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(width < 0 || height < 0 || depth < 0 ||
     width > RSXGL_MAX_TEXTURE_SIZE || height > RSXGL_MAX_TEXTURE_SIZE || depth > RSXGL_MAX_TEXTURE_SIZE) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  // report an error if it were immutable
  if(texture.immutable) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

#if 0
  // TODO - Orphan the texture
  if(texture.valid && texture.timestamp != 0 && (!rsxgl_timestamp_passed(ctx -> cached_timestamp,ctx -> timestamp_sync,texture.timestamp))) {
    texture.timestamp = 0;
  }
#else
  if(texture.valid && texture.timestamp > 0) {
    rsxgl_timestamp_wait(ctx,texture.timestamp);
    texture.timestamp = 0;
  }
#endif

  // set the texture's invalid_storage & valid bits:
  texture.invalid_storage = 1;
  texture.valid = 0;
  texture.dims = 0;

  // set the size for the mipmap level
  texture.levels[level].internalformat = internalformat;
  texture.levels[level].dims = dims;

  texture.levels[level].size[0] = width;
  texture.levels[level].size[1] = height;
  texture.levels[level].size[2] = depth;

  // reset the transfer source:
  texture.levels[level].memory = memory_t();

  // set the arena to allocate from:
  texture.levels[level].arena = ctx -> arena_binding.names[RSXGL_TEXTURE_ARENA];

  // destroy the temporary buffer that may exist for this level:
  if(texture.levels[level].data != 0) {
    free(texture.levels[level].data);
    texture.levels[level].data = 0;

    texture.levels[level].memory = memory_t();
  }

  // - if there is a pixel unpack buffer bound, then:
  if(ctx -> buffer_binding.names[RSXGL_PIXEL_UNPACK_BUFFER] != 0) {
    // TODO - Swizzle and convert pixels, if necessary:
    texture.levels[level].memory = ctx -> buffer_binding[RSXGL_PIXEL_UNPACK_BUFFER].memory + rsxgl_pointer_to_offset(data);
    texture.levels[level].memory.owner = 0;
  }
  // - else if data != 0, then:
  else if(data != 0) {
    //   - allocate intermediate buffer to store the image data, copy data to it
    const uint32_t bytesPerPixel = rsxgl_tex_bytesPerPixel[internalformat];
    const uint32_t pitch = bytesPerPixel * width;
    const size_t nbytes = bytesPerPixel * width * height * depth;
    texture.levels[level].data = malloc(nbytes);
    rsxgl_tex_set_image(internalformat,pitch,texture.levels[level].data,
			format,type,data,
			width,height,depth);
  }

  // for a pixel organized as 0,1,2,3, this appears to transfer channels un-swizzled to an XRGB framebuffer:
  // however this would make the "most efficient" format "GL_ARGB" which doesn't exist:
  //texture.remap = rsxgl_tex_remap(RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,3,2,1,0);
  
  // this makes GL_BGRA the "most efficient" format parameter:
  //texture.remap = rsxgl_tex_remap(RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,0,1,2,3);
  texture.remap = rsxgl_tex_remap(RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_FROM_A,RSXGL_TEXTURE_REMAP_FROM_B,RSXGL_TEXTURE_REMAP_FROM_G,RSXGL_TEXTURE_REMAP_FROM_R);

  ctx -> invalid_textures |= texture.binding_bitfield;

  RSXGL_NOERROR_();
}

static inline void
rsxgl_tex_storage(rsxgl_context_t * ctx,texture_t & texture,const uint8_t dims,GLsizei levels,uint8_t internalformat,GLsizei width,GLsizei height,GLsizei depth)
{
  rsxgl_assert(dims > 0);
  rsxgl_assert(width > 0);
  rsxgl_assert(height > 0);
  rsxgl_assert(depth > 0);

  if(levels < 0 || (boost::static_log2_argument_type)levels > texture_t::max_levels) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(width < 0 || height < 0 || depth < 0 ||
     width > RSXGL_MAX_TEXTURE_SIZE || height > RSXGL_MAX_TEXTURE_SIZE || depth > RSXGL_MAX_TEXTURE_SIZE) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(levels > (log2_uint32(std::max(width,std::max(height,depth))) + 1)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(internalformat == RSXGL_TEX_FORMAT_INVALID) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

#if 0
  // TODO - Orphan the texture
  if(texture.valid && texture.timestamp != 0 && (!rsxgl_timestamp_passed(ctx -> cached_timestamp,ctx -> timestamp_sync,texture.timestamp))) {
    
  }
#else
  if(texture.valid && texture.timestamp > 0) {
    rsxgl_timestamp_wait(ctx,texture.timestamp);
    texture.timestamp = 0;
  }
#endif

  // free any existing texture memory:
  if(texture.memory.offset != 0) {
    rsxgl_arena_free(memory_arena_t::storage().at(texture.arena),texture.memory);
  }

  // free any temporary buffers previously used by the texture:
  texture_t::level_t * plevel = texture.levels;
  for(size_t i = 0;i < texture_t::max_levels;++i,++plevel) {
    if(plevel -> data != 0) {
      free(plevel -> data);
      plevel -> data = 0;
    }
  }

  texture.invalid_storage = 0;
  texture.valid = 1;
  texture.immutable = 1;
  texture.internalformat = internalformat;
  texture.cube = 0;
  texture.rect = 0;
  texture.max_level = levels;
  texture.dims = dims;

  texture.format;

  texture.size[0] = width;
  texture.size[1] = height;
  texture.size[2] = depth;

  const uint32_t bytesPerPixel = rsxgl_tex_bytesPerPixel[internalformat];
  const uint32_t pitch = align_pot< uint32_t, 64 >(width * bytesPerPixel);
  texture.pitch = pitch;

  texture.remap = rsxgl_tex_remap(RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_REMAP,RSXGL_TEXTURE_REMAP_FROM_A,RSXGL_TEXTURE_REMAP_FROM_B,RSXGL_TEXTURE_REMAP_FROM_G,RSXGL_TEXTURE_REMAP_FROM_R);

  texture_t::dimension_size_type unused_size[3];
  uint32_t nbytes = rsxgl_tex_pixel_offset(texture,texture.max_level,0,0,0,unused_size);

  const memory_arena_t::name_type arena = ctx -> arena_binding.names[RSXGL_TEXTURE_ARENA];
  texture.memory = rsxgl_arena_allocate(memory_arena_t::storage().at(arena),128,nbytes,0);
  if(texture.memory.offset == 0) {
    RSXGL_ERROR_(GL_OUT_OF_MEMORY);
  }
  texture.arena = arena;

  texture.format =
    ((texture.memory.location == 0) ? NV30_3D_TEX_FORMAT_DMA0 : NV30_3D_TEX_FORMAT_DMA1) |
    ((texture.cube) ? NV30_3D_TEX_FORMAT_CUBIC : 0) |
    NV30_3D_TEX_FORMAT_NO_BORDER |
    ((uint32_t)texture.dims << NV30_3D_TEX_FORMAT_DIMS__SHIFT) |
    ((uint32_t)(rsx_texture_formats[texture.internalformat] | 0x80 | RSXGL_TEX_FORMAT_LN | (texture.rect ? RSXGL_TEX_FORMAT_UN : 0)) << NV30_3D_TEX_FORMAT_FORMAT__SHIFT) |
    ((uint32_t)texture.max_level << NV40_3D_TEX_FORMAT_MIPMAP_COUNT__SHIFT)
    ;

  ctx -> invalid_textures |= texture.binding_bitfield;

  RSXGL_NOERROR_();
}

static inline void
rsxgl_tex_subimage(rsxgl_context_t * ctx,texture_t & texture,GLint level,GLint x,GLint y,GLint z,GLsizei width,GLsizei height,GLsizei depth,
		   GLenum format,GLenum type,const GLvoid * data)
{
  rsxgl_assert(width > 0);
  rsxgl_assert(height > 0);
  rsxgl_assert(depth > 0);

  if(level < 0 || (boost::static_log2_argument_type)level >= texture_t::max_levels) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(width < 0 || height < 0 || depth < 0 ||
     width > RSXGL_MAX_TEXTURE_SIZE || height > RSXGL_MAX_TEXTURE_SIZE || depth > RSXGL_MAX_TEXTURE_SIZE) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  // GL spec doesn't say what should happen if there's no data source, but clearly need to bail out:
  if(data == 0 && 
     ((ctx -> buffer_binding.names[RSXGL_PIXEL_UNPACK_BUFFER] == 0) || 
      (ctx -> buffer_binding.names[RSXGL_PIXEL_UNPACK_BUFFER] == 0 && ctx -> buffer_binding[RSXGL_PIXEL_UNPACK_BUFFER].memory.offset == 0))) {
    RSXGL_ERROR_(GL_NO_ERROR);
  }

  texture_t::dimension_size_type size[3] = { 0,0,0 };
  uint8_t internalformat = RSXGL_TEX_FORMAT_INVALID;
  uint32_t bytesPerPixel = 0, pitch = 0;

  memory_t srcmem, dstmem;
  const void * srcaddress = 0;
  void * dstaddress = 0;

  // the texture's storage is valid (either rsxgl_tex_storage was called, or a rsxgl_tex_image request has been fulfilled).
  // copy data directly into the texture's buffer:
  if(texture.valid) {
    // TODO - Asynchronous updating.
    if(texture.valid && texture.timestamp > 0) {
      rsxgl_timestamp_wait(ctx,texture.timestamp);
      texture.timestamp = 0;
    }

    internalformat = texture.internalformat;
    bytesPerPixel = rsxgl_tex_bytesPerPixel[internalformat];
    pitch = texture.pitch;

    // there is a pixel buffer object attached - can do DMA:
    if(ctx -> buffer_binding.names[RSXGL_PIXEL_UNPACK_BUFFER] != 0) {
      srcmem = ctx -> buffer_binding[RSXGL_PIXEL_UNPACK_BUFFER].memory + rsxgl_pointer_to_offset(data);
      dstmem = texture.memory + rsxgl_tex_pixel_offset(texture,level,0,0,0,size);
    }
    // source is client memory - just do a memcpy:
    else {
      srcaddress = data;
      dstaddress = (uint8_t *)rsxgl_arena_address(memory_arena_t::storage().at(texture.arena),texture.memory + rsxgl_tex_pixel_offset(texture,level,x,y,z,size));
    }
  }
  // rsxgl_tex_image was called to request that a texture level be allocated, but that hasn't been done yet
  // allocate the temporary buffer for that level if it hasn't been already
  else if(texture.invalid_storage && texture.levels[level].internalformat != RSXGL_TEX_FORMAT_INVALID) {
    // there is a pixel buffer object attached - obtain pointer to it, then memcpy:
    if(ctx -> buffer_binding.names[RSXGL_PIXEL_UNPACK_BUFFER] != 0) {
      srcaddress = rsxgl_arena_address(memory_arena_t::storage().at(ctx -> buffer_binding[RSXGL_PIXEL_UNPACK_BUFFER].arena),
				       ctx -> buffer_binding[RSXGL_PIXEL_UNPACK_BUFFER].memory + rsxgl_pointer_to_offset(data));
    }
    // source is client memory:
    else {
      srcaddress = data;
    }

    size[0] = texture.levels[level].size[0];
    size[1] = texture.levels[level].size[1];
    size[2] = texture.levels[level].size[2];

    internalformat = texture.levels[level].internalformat;
    bytesPerPixel = rsxgl_tex_bytesPerPixel[internalformat];
    pitch = bytesPerPixel * size[0];

    if(texture.levels[level].data == 0) {
      const size_t nbytes = pitch * size[1] * size[2];
      texture.levels[level].data = malloc(nbytes);
    }
    dstaddress = texture.levels[level].data;
  }
  else {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if((x + width) > size[0] || (y + height) > size[1] || (z + depth) > size[2]) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(srcaddress != 0 && dstaddress != 0) {
    rsxgl_tex_set_image(internalformat,pitch,dstaddress,
			format,type,srcaddress,
			width,height,depth);
  }
  else if(srcmem && dstmem) {
    rsxgl_tex_transfer_image(ctx -> base.gcm_context,
			     dstmem,pitch,
			     x,y,z,
			     srcmem,width * bytesPerPixel,
			     0,0,0,
			     width,height,depth,bytesPerPixel);
  }

   RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glTexImage1D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
  if(!(target == GL_TEXTURE_1D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  uint8_t rsx_format = rsxgl_tex_format(internalformat);
  rsxgl_tex_image(ctx,texture,1,level,rsx_format,std::max(width,1),1,1,format,type,pixels);
}

GLAPI void APIENTRY
glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
  if(!(target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_RECTANGLE ||
       target == GL_TEXTURE_1D_ARRAY)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  uint8_t rsx_format = rsxgl_tex_format(internalformat);
  rsxgl_tex_image(ctx,texture,2,level,rsx_format,std::max(width,1),std::max(height,1),1,format,type,pixels);
}

GLAPI void APIENTRY
glTexImage3D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
  if(!(target == GL_TEXTURE_3D ||
       target == GL_TEXTURE_2D_ARRAY)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  uint8_t rsx_format = rsxgl_tex_format(internalformat);
  rsxgl_tex_image(ctx,texture,3,level,rsx_format,std::max(width,1),std::max(height,1),std::max(depth,1),format,type,pixels);
}

GLAPI void APIENTRY
glTexStorage1D(GLenum target, GLsizei levels,GLenum internalformat,GLsizei width)
{
  if(!(target == GL_TEXTURE_1D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  uint8_t rsx_format = rsxgl_tex_format(internalformat);
  rsxgl_tex_storage(ctx,texture,1,levels,rsx_format,std::max(width,1),1,1);
}

GLAPI void APIENTRY
glTexStorage2D(GLenum target, GLsizei levels,GLenum internalformat,GLsizei width, GLsizei height)
{
  if(!(target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_RECTANGLE ||
       target == GL_TEXTURE_1D_ARRAY)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  uint8_t rsx_format = rsxgl_tex_format(internalformat);
  rsxgl_tex_storage(ctx,texture,2,levels,rsx_format,std::max(width,1),std::max(height,1),1);
}

GLAPI void APIENTRY
glTexStorage3D(GLenum target, GLsizei levels,GLenum internalformat,GLsizei width, GLsizei height, GLsizei depth)
{
  if(!(target == GL_TEXTURE_3D ||
       target == GL_TEXTURE_2D_ARRAY)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  uint8_t rsx_format = rsxgl_tex_format(internalformat);
  rsxgl_tex_storage(ctx,texture,3,levels,rsx_format,std::max(width,1),std::max(height,1),std::max(depth,1));
}

GLAPI void APIENTRY
glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels,GLenum internalformat,GLsizei width)
{
}

GLAPI void APIENTRY
glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels,GLenum internalformat,GLsizei width, GLsizei height)
{
}

GLAPI void APIENTRY
glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels,GLenum internalformat,GLsizei width, GLsizei height, GLsizei depth)
{
}

GLAPI void APIENTRY
glGetTexImage (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
}

GLAPI void APIENTRY
glGetTexParameterfv (GLenum target, GLenum pname, GLfloat *params)
{
  if(!(target == GL_TEXTURE_1D ||
       target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_3D ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_RECTANGLE)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  rsxgl_get_texture_parameterf(ctx,ctx -> texture_binding.names[ctx -> active_texture],pname,params);
}

GLAPI void APIENTRY
glGetTexParameteriv (GLenum target, GLenum pname, GLint *params)
{
  if(!(target == GL_TEXTURE_1D ||
       target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_3D ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_RECTANGLE)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  uint32_t value = 0;
  rsxgl_get_texture_parameteri(ctx,ctx -> texture_binding.names[ctx -> active_texture],pname,&value);
  *params = value;
}

GLAPI void APIENTRY
glGetTexLevelParameterfv (GLenum target, GLint level, GLenum pname, GLfloat *params)
{
}

GLAPI void APIENTRY
glGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname, GLint *params)
{
}

GLAPI void APIENTRY
glCopyTexImage1D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
}

GLAPI void APIENTRY
glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
}

GLAPI void APIENTRY
glCopyTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
}

GLAPI void APIENTRY
glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
}

GLAPI void APIENTRY
glCopyTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
}

GLAPI void APIENTRY
glTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
  if(!(target == GL_TEXTURE_1D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_tex_subimage(ctx,texture,level,xoffset,0,0,width,1,1,format,type,pixels);
}

GLAPI void APIENTRY
glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
  if(!(target == GL_TEXTURE_2D ||
       target == GL_TEXTURE_CUBE_MAP ||
       target == GL_TEXTURE_RECTANGLE)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_tex_subimage(ctx,texture,level,xoffset,yoffset,0,width,height,1,format,type,pixels);
}

GLAPI void APIENTRY
glTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels)
{
  if(!(target == GL_TEXTURE_3D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_tex_subimage(ctx,texture,level,xoffset,yoffset,zoffset,width,height,depth,format,type,pixels);
}

GLAPI void APIENTRY
glCompressedTexImage3D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data)
{
}

GLAPI void APIENTRY
glCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data)
{
}

GLAPI void APIENTRY
glCompressedTexImage1D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data)
{
}

GLAPI void APIENTRY
glCompressedTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data)
{
}

GLAPI void APIENTRY
glCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data)
{
}

GLAPI void APIENTRY
glCompressedTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data)
{
}

GLAPI void APIENTRY
glGetCompressedTexImage (GLenum target, GLint level, GLvoid *img)
{
}

GLAPI void APIENTRY
glTexBuffer (GLenum target, GLenum internalformat, GLuint buffer)
{
}

void
rsxgl_texture_validate(rsxgl_context_t * ctx,texture_t & texture)
{
  // storage is invalid:
  if(texture.invalid_storage) {
    //rsxgl_debug_printf("\t\tinvalid_storage: dims: %u\n",texture.dims);
    
    // destroy whatever is already there:
    if(texture.memory.offset != 0) {
      rsxgl_arena_free(memory_arena_t::storage().at(texture.arena),texture.memory);
      texture.memory = memory_t();
    }
    
    uint32_t nbytes = 0, bytesPerPixel = 0,pitch = 0, format = RSXGL_TEX_FORMAT_INVALID;
    memory_arena_t::name_type arena = 0;
    
    uint8_t dims = 0;
    texture_t::level_t * plevel = texture.levels;
    size_t level = 0, levels = 0, ntransfer = 0;
    texture_t::dimension_size_type expected_size[3] = { 0,0,0 };
    for(;level < texture_t::max_levels;++level,++plevel) {
#if 0
      rsxgl_debug_printf("\t\t\t%u: format:%x %ux%ux%u\n",
			 level,
			 plevel -> internalformat,
			 plevel -> size[0],plevel -> size[1],plevel -> size[2]);
#endif
      if(plevel -> internalformat == RSXGL_TEX_FORMAT_INVALID && plevel -> dims == 0) break;
      
      // is the size what we expected? if not, fail:
      if(levels > 0 && (plevel -> internalformat != format ||
			plevel -> dims != dims ||
			plevel -> size[0] != expected_size[0] || plevel -> size[1] != expected_size[1] || plevel -> size[2] ||
			plevel -> arena != arena)) break;
      
      // fill in the expected size:
      if(levels == 0) {
	dims = plevel -> dims;
	format = plevel -> internalformat;
	bytesPerPixel = rsxgl_tex_bytesPerPixel[format];
	
	// pitch is a multiple of 64, to make it compatible with framebuffer objects:
	pitch = align_pot< uint32_t, 64 >(plevel -> size[0] * bytesPerPixel);
	arena = plevel -> arena;
	
	expected_size[0] = std::max(plevel -> size[0] >> 1,1);
	expected_size[1] = std::max(plevel -> size[1] >> 1,1);
	expected_size[2] = std::max(plevel -> size[2] >> 1,1);
      }
      else {
	expected_size[0] = std::max(expected_size[0] >> 1,1);
	expected_size[1] = std::max(expected_size[1] >> 1,1);
	expected_size[2] = std::max(expected_size[2] >> 1,1);
      }
      
      // accumulate storage requirements:
      nbytes += pitch * plevel -> size[1] * plevel -> size[2];
      
      if(plevel -> data != 0 || plevel -> memory.offset != 0) ++ntransfer;
      
      ++levels;
    }
    
    // successfully specified the texture levels, now allocate:
    if(levels > 0) {
      rsx_ptr_t address = 0;
      texture.memory = rsxgl_arena_allocate(memory_arena_t::storage().at(arena),128,nbytes,&address);
      
      if(texture.memory) {
	texture.valid = 1;
	texture.internalformat = format;
	texture.max_level = levels;
	texture.dims = dims;
	texture.size[0] = texture.levels[0].size[0];
	texture.size[1] = texture.levels[0].size[1];
	texture.size[2] = texture.levels[0].size[2];
	texture.pitch = pitch;
	texture.arena = arena;
	
	texture.format =
	  ((texture.memory.location == 0) ? NV30_3D_TEX_FORMAT_DMA0 : NV30_3D_TEX_FORMAT_DMA1) |
	  ((texture.cube) ? NV30_3D_TEX_FORMAT_CUBIC : 0) |
	  NV30_3D_TEX_FORMAT_NO_BORDER |
	  ((uint32_t)texture.dims << NV30_3D_TEX_FORMAT_DIMS__SHIFT) |
	  ((uint32_t)(rsx_texture_formats[texture.internalformat] | 0x80 | RSXGL_TEX_FORMAT_LN | (texture.rect ? RSXGL_TEX_FORMAT_UN : 0)) << NV30_3D_TEX_FORMAT_FORMAT__SHIFT) |
	  ((uint32_t)texture.max_level << NV40_3D_TEX_FORMAT_MIPMAP_COUNT__SHIFT)
	  ;
	
#if 0	      
	rsxgl_debug_printf("\t\tsuccess dims: %u format: %u (%x) size: %u,%u,%u bytes: %u pitch: %u max_level: %u remap: %x offset: %u\n",
			   texture.dims,
			   texture.internalformat,(uint32_t)rsx_texture_formats[texture.internalformat],
			   texture.size[0],texture.size[1],texture.size[2],
			   nbytes,texture.pitch,
			   texture.max_level,
			   texture.remap,
			   texture.memory.offset);
#endif
	
	// upload initial texture values:
	if(ntransfer > 0) {
	  gcmContextData * context = ctx -> gcm_context();

	  uint32_t offset = 0;
	  texture_t::level_t * plevel = texture.levels;
	  texture_t::dimension_size_type size[3] = { texture.size[0],texture.size[1],texture.size[2] };
	  for(level = 0;level < levels;++level,++plevel) {
	    // transfer from main memory:
	    if(plevel -> data != 0) {
	      rsxgl_tex_copy_image((uint8_t *)address + offset,pitch,0,0,0,
				   plevel -> data,bytesPerPixel * plevel -> size[0],0,0,0,
				   size[0],size[1],size[2],bytesPerPixel);
	    }
	    // transfer with RSX DMA:
	    else if(plevel -> memory.offset != 0) {
	      //rsxgl_debug_printf("\t\ttransfer from RSX memory offset: %u:%u\n",texture.levels[level].memory.location,texture.levels[level].memory.offset);
	      rsxgl_tex_transfer_image(context,
				       texture.memory + offset,pitch,0,0,0,
				       plevel -> memory,bytesPerPixel * plevel -> size[0],0,0,0,
				       size[0],size[1],size[2],bytesPerPixel);
	    }
	    
	    offset += pitch * size[1] * size[2];
	    
	    size[0] = std::max(size[0] >> 1,1);
	    size[1] = std::max(size[1] >> 1,1);
	    size[2] = std::max(size[2] >> 1,1);
	  }
	}
      }
      else {
	texture.valid = 0;
      }
    }
    // failure:
    else {
      //rsxgl_debug_printf("\t\tfailure: %u %u\n",levels,level);
      
      texture.valid = 0;
    }
    
    // always set this to 0:
    texture.invalid_storage = 0;
  }  
}

void
rsxgl_textures_validate(rsxgl_context_t * ctx,program_t & program,const uint32_t timestamp)
{
  gcmContextData * context = ctx -> base.gcm_context;

  const bit_set< RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS > program_textures = program.textures_enabled;

  const bit_set< RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS >
    invalid_textures = program_textures & ctx -> invalid_textures,
    invalid_samplers = program_textures & ctx -> invalid_samplers;

  // Invalidate the texture cache.
  // TODO - determine when this is necessary to do, and only do it then.
  {
    uint32_t * buffer = gcm_reserve(context,4);

    // Fragment program textures:
    gcm_emit_method_at(buffer,0,NV40_3D_TEX_CACHE_CTL,1);
    gcm_emit_at(buffer,1,1);

    // Vertex program textures:
    gcm_emit_method_at(buffer,2,NV40_3D_TEX_CACHE_CTL,1);
    gcm_emit_at(buffer,3,2);

    gcm_finish_n_commands(context,4);
  }

  // Update texture timestamps:
  for(size_t i = 0;i < RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;++i) {
    if(!program_textures.test(i)) continue;
    const texture_t::name_type name = ctx -> texture_binding.names[i];
    if(name == 0) continue;
    texture_t & texture = ctx -> texture_binding[i];
    rsxgl_assert(timestamp >= texture.timestamp);
    texture.timestamp = timestamp;
  }

  if(invalid_samplers.any()) {
    //rsxgl_debug_printf("%s: samplers\n",__PRETTY_FUNCTION__);
    for(size_t i = 0;i < RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;++i) {
      if(invalid_samplers.test(i)) {
	//rsxgl_debug_printf("\t%u\n",i);
	const sampler_t & sampler = (ctx -> sampler_binding.names[i] != 0) ? ctx -> sampler_binding[i] : ctx -> texture_binding[i].sampler;

	const uint32_t wrap = 
	  ((uint32_t)(sampler.wrap_s + 1) << NV30_3D_TEX_WRAP_S__SHIFT) |
	  ((uint32_t)(sampler.wrap_t + 1) << NV30_3D_TEX_WRAP_T__SHIFT) |
	  ((uint32_t)(sampler.wrap_r + 1) << NV30_3D_TEX_WRAP_R__SHIFT)
	  ;

	const uint32_t compare =
	  (uint32_t)sampler.compare_func << NV30_3D_TEX_WRAP_RCOMP__SHIFT
	  ;
	
	const uint32_t filter =
	  ((uint32_t)(sampler.filter_min + 1) << NV30_3D_TEX_FILTER_MIN__SHIFT) |
	  ((uint32_t)(sampler.filter_mag + 1) << NV30_3D_TEX_FILTER_MAG__SHIFT) |
	  // "convolution":
	  ((uint32_t)1 << 13)
	  ;
	
	//
	uint32_t * buffer = gcm_reserve(context,4);
	
	gcm_emit_method(&buffer,NV30_3D_TEX_FILTER(i),1);
	gcm_emit(&buffer,filter);
	
	gcm_emit_method(&buffer,NV30_3D_TEX_WRAP(i),1);
	gcm_emit(&buffer,wrap | compare);
	
	gcm_finish_commands(context,&buffer);
      }
    }

    ctx -> invalid_samplers &= ~invalid_samplers;
  }

  if(invalid_textures.any()) {
    gcmContextData * context = ctx -> base.gcm_context;

    //rsxgl_debug_printf("%s: textures\n",__PRETTY_FUNCTION__);
    for(size_t i = 0;i < RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;++i) {
      if(invalid_textures.test(i)) {
	//rsxgl_debug_printf("\t%u\n",i);
	texture_t & texture = ctx -> texture_binding[i];

	rsxgl_texture_validate(ctx,texture);

	if(texture.valid) {
	  // activate the texture:
	  uint32_t * buffer = gcm_reserve(context,11);
	  
	  gcm_emit_method(&buffer,NV30_3D_TEX_OFFSET(i),2);
	  gcm_emit(&buffer,texture.memory.offset);
	  gcm_emit(&buffer,texture.format);
	  
	  gcm_emit_method(&buffer,NV30_3D_TEX_ENABLE(i),1);
	  gcm_emit(&buffer,NV40_3D_TEX_ENABLE_ENABLE);
	  
	  gcm_emit_method(&buffer,NV30_3D_TEX_NPOT_SIZE(i),1);
	  gcm_emit(&buffer,((uint32_t)texture.size[0] << NV30_3D_TEX_NPOT_SIZE_W__SHIFT) | (uint32_t)texture.size[1]);
	  
	  gcm_emit_method(&buffer,NV40_3D_TEX_SIZE1(i),1);
	  gcm_emit(&buffer,((uint32_t)texture.size[2] << NV40_3D_TEX_SIZE1_DEPTH__SHIFT) | (uint32_t)texture.pitch);
	  
	  gcm_emit_method(&buffer,NV30_3D_TEX_SWIZZLE(i),1);
	  gcm_emit(&buffer,texture.remap);
	  
	  gcm_finish_commands(context,&buffer);
	}
      }
    }

    ctx -> invalid_textures &= ~invalid_textures;
  }
}
