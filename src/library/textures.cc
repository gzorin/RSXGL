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
#include "texture_migrate.h"

#include <GL3/gl3.h>
#include "GL3/gl3ext.h"
#include "error.h"

#include <rsx/gcm_sys.h>
#include "nv40.h"
#include "gl_fifo.h"
#include "cxxutil.h"
#include "timestamp.h"

#include "pipe/p_defines.h"
#include "util/u_format.h"

extern "C" {
#include "nvfx/nvfx_tex.h"
}

#include <malloc.h>
#include <string.h>

extern "C" {
  enum pipe_format
  rsxgl_choose_format(struct pipe_screen *screen, GLenum internalFormat,
		      GLenum format, GLenum type,
		      enum pipe_texture_target target, unsigned sample_count,
		      unsigned bindings);

  enum pipe_format
  rsxgl_choose_source_format(const GLenum format,const GLenum type);

  const struct nvfx_texture_format *
  nvfx_get_texture_format(enum pipe_format format);

  uint32_t
  nvfx_get_texture_remap(const struct nvfx_texture_format * tf,uint8_t r,uint8_t g,uint8_t b,uint8_t a);
}

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

sampler_t::storage_type & sampler_t::storage()
{
  return current_object_ctx() -> sampler_storage();
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
    const GLuint sampler_name = *samplers;

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

texture_t::storage_type & texture_t::storage()
{
  return current_object_ctx() -> texture_storage();
}

texture_t::texture_t()
  : deleted(0), timestamp(0), ref_count(0),
    invalid(0), invalid_complete(0),
    complete(0), immutable(0),
    cube(0), rect(0), num_levels(0), dims(0), pformat(PIPE_FORMAT_NONE), format(0), pitch(0), remap(0)
{
  swizzle.r = RSXGL_TEXTURE_SWIZZLE_FROM_R;
  swizzle.g = RSXGL_TEXTURE_SWIZZLE_FROM_G;
  swizzle.b = RSXGL_TEXTURE_SWIZZLE_FROM_B;
  swizzle.a = RSXGL_TEXTURE_SWIZZLE_FROM_A;

  size[0] = 0;
  size[1] = 0;
  size[2] = 0;
}

texture_t::~texture_t()
{
  if(memory.owner && memory) {
    rsxgl_arena_free(memory_arena_t::storage().at(arena),memory);
  }
}

texture_t::level_t::level_t()
  : dims(0), cube(0), rect(0), pformat(PIPE_FORMAT_NONE), pitch(0), memory(RSXGL_TEXTURE_MIGRATE_BUFFER_LOCATION,0,1)
{
  size[0] = 0;
  size[1] = 0;
  size[2] = 0;
}

texture_t::level_t::~level_t()
{
  if(memory.owner && memory) {
    rsxgl_texture_migrate_free(rsxgl_texture_migrate_address(memory.offset));
  }
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
    const GLuint texture_name = *textures;

    if(texture_name == 0) continue;

    // Free resources used by this object:
    if(texture_t::storage().is_object(texture_name)) {
      ctx -> texture_binding.unbind_from_all(texture_name);

      // TODO: orphan it, instead of doing this:
      texture_t & texture = texture_t::storage().at(texture_name);
      if(texture.timestamp > 0) {
	rsxgl_timestamp_wait(ctx,texture.timestamp);
	texture.timestamp = 0;
      }

      texture_t::gl_object_type::maybe_delete(texture_name);
    }
    else if(texture_t::storage().is_name(texture_name)) {
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

static inline uint8_t
rsxgl_texture_target_dims(const GLenum target)
{
  if(target == GL_TEXTURE_1D || target == GL_TEXTURE_1D_ARRAY) {
    return 1;
  }
  else if(target == GL_TEXTURE_2D || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_RECTANGLE || target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
    return 2;
  }
  else if(target == GL_TEXTURE_3D) {
    return 3;
  }
  else {
    return 0;
  }
}

GLAPI void APIENTRY
glBindTexture (GLenum target, GLuint texture_name)
{
  const uint8_t dims = rsxgl_texture_target_dims(target);

  if(dims == 0) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!(texture_name == 0 || texture_t::storage().is_name(texture_name))) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(texture_name != 0) {
    if(!texture_t::storage().is_object(texture_name)) {
      texture_t::storage().create_object(texture_name);
      texture_t::storage().at(texture_name).dims = dims;
    }
    else if(texture_t::storage().at(texture_name).dims != dims) {
      RSXGL_ERROR_(GL_INVALID_OPERATION);
    }
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
    uint16_t from = RSXGL_TEXTURE_SWIZZLE_ZERO;

    switch((int)param) {
    case GL_RED:
      from = RSXGL_TEXTURE_SWIZZLE_FROM_R;
      break;
    case GL_GREEN:
      from = RSXGL_TEXTURE_SWIZZLE_FROM_G;
      break;
    case GL_BLUE:
      from = RSXGL_TEXTURE_SWIZZLE_FROM_B;
      break;
    case GL_ALPHA:
      from = RSXGL_TEXTURE_SWIZZLE_FROM_A;
      break;
    case GL_ONE:
      from = RSXGL_TEXTURE_SWIZZLE_ONE;
    default:
      from = RSXGL_TEXTURE_SWIZZLE_ZERO;
    }

    switch(pname) {
    case GL_TEXTURE_SWIZZLE_R:
      texture.swizzle.r = from;
      break;
    case GL_TEXTURE_SWIZZLE_G:
      texture.swizzle.g = from;
      break;
    case GL_TEXTURE_SWIZZLE_B:
      texture.swizzle.b = from;
      break;
    case GL_TEXTURE_SWIZZLE_A:
      texture.swizzle.a = from;
      break;
    }

    texture.remap = nvfx_get_texture_remap(nvfx_get_texture_format(texture.pformat),
					   texture.swizzle.r,texture.swizzle.g,texture.swizzle.b,texture.swizzle.a);

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
    uint16_t from = RSXGL_TEXTURE_SWIZZLE_ZERO;

    switch(pname) {
    case GL_TEXTURE_SWIZZLE_R:
      from = texture.swizzle.r;
      break;
    case GL_TEXTURE_SWIZZLE_G:
      from = texture.swizzle.g;
      break;
    case GL_TEXTURE_SWIZZLE_B:
      from = texture.swizzle.b;
      break;
    case GL_TEXTURE_SWIZZLE_A:
      from = texture.swizzle.a;
      break;
    }

    switch(from) {
    case RSXGL_TEXTURE_SWIZZLE_FROM_R:
      *param = GL_RED;
      break;
    case RSXGL_TEXTURE_SWIZZLE_FROM_G:
      *param = GL_GREEN;
      break;
    case RSXGL_TEXTURE_SWIZZLE_FROM_B:
      *param = GL_BLUE;
      break;
    case RSXGL_TEXTURE_SWIZZLE_FROM_A:
      *param = GL_ALPHA;
      break;
    case RSXGL_TEXTURE_SWIZZLE_ONE:
      *param = GL_ONE;
      break;
    default:
      *param = GL_ZERO;
      break;
    }
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

static inline uint32_t
rsxgl_get_tex_level_offset_size(const texture_t::dimension_size_type _size[3],
				const uint32_t pitch,
				const texture_t::level_size_type level,
				texture_t::dimension_size_type * outsize)
{
  texture_t::dimension_size_type size[3] = { _size[0], _size[1], _size[2] };
  uint32_t offset = 0;

  for(texture_t::level_size_type i = 1;i <= level;++i) {
    offset += pitch * size[1] * size[2];

    for(int j = 0;j < 3;++j) {
      size[j] = std::max(size[j] >> 1,1);
    }
  }

  if(outsize != 0) {
    for(int j = 0;j < 3;++j) {
      outsize[j] = size[j];
    }
  }

  return offset;
}

// Meant to look like gallium's util_format_translate, but tries to use DMA:
static inline void
rsxgl_util_format_translate_dma(rsxgl_context_t * ctx,
				enum pipe_format dst_format,
				void * dstaddress, const memory_t & dstmem, unsigned dst_stride,
				unsigned dst_x, unsigned dst_y,
				enum pipe_format src_format,
				void * srcaddress, const memory_t & srcmem, unsigned src_stride,
				unsigned src_x, unsigned src_y,
				unsigned width, unsigned height)
{
  const struct util_format_description *dst_format_desc = util_format_description(dst_format);
  const struct util_format_description *src_format_desc = util_format_description(src_format);

  if(util_is_format_compatible(src_format_desc, dst_format_desc)) {
    const int blocksize = dst_format_desc->block.bits / 8;
    const int blockwidth = dst_format_desc->block.width;
    const int blockheight = dst_format_desc->block.height;
    
    dst_x /= blockwidth;
    dst_y /= blockheight;
    width = (width + blockwidth - 1)/blockwidth;
    height = (height + blockheight - 1)/blockheight;
    src_x /= blockwidth;
    src_y /= blockheight;
    
    const uint32_t linelength = width * blocksize;
    
    rsxgl_memory_transfer(ctx -> gcm_context(),
			  dstmem + (dst_x * blocksize) + (dst_y * dst_stride),dst_stride,1,
			  srcmem + (src_x * blocksize) + (src_y * src_stride),src_stride,1,
			  linelength,height);
  }
  else {
    rsxgl_debug_printf("%s: non-trivial transfer didn't happen src:%x dst:%x\n",__PRETTY_FUNCTION__,
		       (unsigned int)src_format,(unsigned int)dst_format);

    rsxgl_assert(dstaddress != 0);
    rsxgl_assert(srcaddress != 0);

    util_format_translate(dst_format,dstaddress,dst_stride,dst_x,dst_y,
			  src_format,srcaddress,src_stride,src_x,src_y,
			  width,height);
  }
}

void
rsxgl_texture_validate_complete(rsxgl_context_t * ctx,texture_t & texture)
{
  if(texture.invalid_complete) {
    texture_t::level_t * plevel = texture.levels;

    // Check the first level:
    uint8_t dims = plevel -> dims;
    pipe_format pformat = plevel -> pformat;

    bool complete = (dims == texture.dims) && (pformat != PIPE_FORMAT_NONE);

    bool cube = plevel -> cube, rect = plevel -> rect;
    ++plevel;

    // Check the remaining levels:
    texture_t::level_size_type level = 1;
    
    for(;complete && level < texture_t::max_levels;++level) {
      // This level is unspecified; break
      if(plevel -> dims == 0 || plevel -> pformat == 0) {
	break;
      }
      // Level has incompatible dimensions, format:
      else if((plevel -> dims != dims) || (plevel -> cube != cube) || (plevel -> rect != rect) ||
	      (!util_is_format_compatible(util_format_description(pformat),
					  util_format_description(plevel -> pformat)))) {
	complete = false;
	break;
      }

      ++plevel;
    }

    texture.complete = complete;

    if(complete) {
      texture.pformat = pformat;
      texture.size[0] = texture.levels[0].size[0];
      texture.size[1] = texture.levels[0].size[1];
      texture.size[2] = texture.levels[0].size[2];
      texture.cube = cube;
      texture.rect = rect;
      texture.num_levels = level;
    }
    else {
      texture.pformat = PIPE_FORMAT_NONE;
      texture.size[0] = 0;
      texture.size[1] = 0;
      texture.size[2] = 0;
      texture.cube = false;
      texture.rect = false;
      texture.num_levels = 0;
    }

    texture.invalid_complete = 0;
  }
}

static inline void
rsxgl_texture_validate_storage(rsxgl_context_t * ctx,texture_t & texture)
{
  rsxgl_assert(!texture.memory);
  rsxgl_assert(texture.complete);
  rsxgl_assert(texture.dims != 0);
  rsxgl_assert(texture.pformat != PIPE_FORMAT_NONE);

  // pitch is aligned to 64 bytes so it can be attached to a framebuffer:
  const uint32_t pitch_tmp = util_format_get_stride(texture.pformat,texture.size[0]);
  const uint32_t pitch = texture.dims > 1 ? align_pot< uint32_t, 64 >(pitch_tmp) : pitch_tmp;

  uint32_t nbytes = 0;
  texture_t::dimension_size_type size[3] = { texture.size[0], texture.size[1], texture.size[2] };
  for(texture_t::level_size_type i = 0,n = texture.num_levels;i < n;++i) {
    nbytes += pitch * size[1] * size[2];

    for(int j = 0;j < 3;++j) {
      size[j] = std::max(size[j] >> 1,1);
    }
  }

  texture.memory = rsxgl_arena_allocate(memory_arena_t::storage().at(texture.arena),128,nbytes,0);
  texture.memory.owner = true;

  if(texture.memory) {
    const nvfx_texture_format * pfmt = nvfx_get_texture_format(texture.pformat);
    rsxgl_assert(pfmt != 0);
    
    const uint32_t fmt = pfmt -> fmt[4] | NV40_3D_TEX_FORMAT_LINEAR | (texture.rect ? NV40_3D_TEX_FORMAT_RECT : 0) | 0x8000;
    
#if 0
    rsxgl_debug_printf("%s: dims:%u pformat:%u size:%ux%ux%u pitch:%u levels:%u bytes:%u fmt:%x\n",__PRETTY_FUNCTION__,
		       (unsigned int)texture.dims,
		       (unsigned int)texture.pformat,
		       (unsigned int)texture.size[0],(unsigned int)texture.size[1],(unsigned int)texture.size[2],
		       (unsigned int)pitch,
		       (unsigned int)texture.num_levels,
		       (unsigned int)nbytes,(unsigned int)fmt);
    rsxgl_debug_printf("\toffset:%u\n",texture.memory.offset);
#endif
    
    texture.format =
      ((texture.memory.location == 0) ? NV30_3D_TEX_FORMAT_DMA0 : NV30_3D_TEX_FORMAT_DMA1) |
      ((texture.cube) ? NV30_3D_TEX_FORMAT_CUBIC : 0) |
      NV30_3D_TEX_FORMAT_NO_BORDER |
      ((uint32_t)texture.dims << NV30_3D_TEX_FORMAT_DIMS__SHIFT) |
      fmt |
      ((uint32_t)texture.num_levels << NV40_3D_TEX_FORMAT_MIPMAP_COUNT__SHIFT)
      ;

    texture.pitch = pitch;
    
    texture.remap = nvfx_get_texture_remap(pfmt,
					   texture.swizzle.r,texture.swizzle.g,texture.swizzle.b,texture.swizzle.a);
  }
}

static inline void
rsxgl_texture_reset_storage(texture_t & texture)
{
  if(texture.memory && texture.memory.owner) {
    rsxgl_arena_free(memory_arena_t::storage().at(texture.arena),texture.memory);
  }
  
  texture.format = 0;
  texture.pitch = 0;
  texture.remap = 0;
  texture.memory = memory_t();
}

static inline void
rsxgl_texture_level_format(texture_t::level_t & level,const uint8_t dims,const pipe_format pformat,
			   const texture_t::dimension_size_type width,const texture_t::dimension_size_type height,const texture_t::dimension_size_type depth)
{
  level.dims = dims;
  level.pformat = pformat;
  level.size[0] = width;
  level.size[1] = height;
  level.size[2] = depth;
  level.pitch = util_format_get_stride(pformat,width);
}

static inline void
rsxgl_texture_level_validate_storage(texture_t::level_t & level)
{
  rsxgl_assert(!level.memory);
  rsxgl_assert(level.dims != 0);
  rsxgl_assert(level.pformat != PIPE_FORMAT_NONE);

  const size_t nbytes = util_format_get_2d_size(level.pformat,level.pitch,level.size[1]) * level.size[2];

  void * ptr = rsxgl_texture_migrate_memalign(16,nbytes);

  level.memory.location = RSXGL_TEXTURE_MIGRATE_BUFFER_LOCATION;
  level.memory.offset = rsxgl_texture_migrate_offset(ptr);
  level.memory.owner = 1;
}

static inline void
rsxgl_texture_level_reset_storage(texture_t::level_t & level)
{
  if(level.memory.owner && level.memory) {
    rsxgl_texture_migrate_free(rsxgl_texture_migrate_address(level.memory.offset));
  }
  
  level.pformat = PIPE_FORMAT_NONE;
  level.size[0] = 0;
  level.size[1] = 0;
  level.size[2] = 0;
  level.pitch = 0;
  level.memory = memory_t();
}

static inline void
rsxgl_tex_storage(rsxgl_context_t * ctx,texture_t & texture,const uint8_t dims,const bool cube,const bool rect,GLsizei levels,GLint glinternalformat,GLsizei width,GLsizei height,GLsizei depth)
{
  rsxgl_assert(dims > 0);
  rsxgl_assert(width > 0);
  rsxgl_assert(height > 0);
  rsxgl_assert(depth > 0);

  if(texture.immutable) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

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

#if 0
  // TODO: Orphan the texture
  if(texture.timestamp != 0 && (!rsxgl_timestamp_passed(ctx -> cached_timestamp,ctx -> timestamp_sync,texture.timestamp))) {
  }
#else
  if(texture.timestamp > 0) {
    rsxgl_timestamp_wait(ctx,texture.timestamp);
    texture.timestamp = 0;
  }
#endif

  rsxgl_texture_reset_storage(texture);
  for(size_t i = 0;i < texture_t::max_levels;++i) {
    rsxgl_texture_level_reset_storage(texture.levels[i]);
  }

  texture.invalid = 0;
  texture.invalid_complete = 0;
  texture.immutable = 1;

  ctx -> invalid_textures |= texture.binding_bitfield;

  const pipe_format pformat = rsxgl_choose_format(ctx -> screen(),
						  glinternalformat,GL_NONE,GL_NONE,
						  (dims == 1) ? PIPE_TEXTURE_1D :
						  (dims == 2) ? (cube ? PIPE_TEXTURE_CUBE : (rect ? PIPE_TEXTURE_RECT : PIPE_TEXTURE_2D)) :
						  (dims == 3) ? PIPE_TEXTURE_2D :
						  PIPE_MAX_TEXTURE_TYPES,
						  1,
						  PIPE_BIND_SAMPLER_VIEW);

  if(pformat == PIPE_FORMAT_NONE) {
    texture.complete = 0;
    texture.pformat = PIPE_FORMAT_NONE;
    texture.cube = false;
    texture.rect = false;
    texture.num_levels = 0;
    
    texture.size[0] = 0;
    texture.size[1] = 0;
    texture.size[2] = 0;

    RSXGL_ERROR_(GL_INVALID_ENUM);
  }
  else {
    texture.complete = 1;
    texture.pformat = pformat;
    texture.cube = cube;
    texture.rect = rect;
    texture.num_levels = levels;
    
    texture.size[0] = width;
    texture.size[1] = height;
    texture.size[2] = depth;

    texture.arena = ctx -> arena_binding.names[RSXGL_TEXTURE_ARENA];
    
    rsxgl_texture_validate_storage(ctx,texture);
    
    RSXGL_NOERROR_();
  }
}

static inline bool
rsxgl_tex_image_format(rsxgl_context_t * ctx,texture_t & texture,const uint8_t dims,const bool cube,const bool rect,GLint _level,GLint glinternalformat,GLsizei width,GLsizei height,GLsizei depth)
{
  rsxgl_assert(dims > 0);
  rsxgl_assert(width > 0);
  rsxgl_assert(height > 0);
  rsxgl_assert(depth > 0);

  // report an error if it were immutable
  if(texture.immutable) {
    RSXGL_ERROR(GL_INVALID_OPERATION,false);
  }

  if(_level < 0 || (boost::static_log2_argument_type)_level >= texture_t::max_levels) {
    RSXGL_ERROR(GL_INVALID_VALUE,false);
  }

  const pipe_format pdstformat = (texture.levels[0].pformat != PIPE_FORMAT_NONE) ?
    texture.levels[0].pformat :
    rsxgl_choose_format(ctx -> screen(),
			glinternalformat,GL_NONE,GL_NONE,
			(dims == 1) ? PIPE_TEXTURE_1D :
			(dims == 2) ? (cube ? PIPE_TEXTURE_CUBE : (rect ? PIPE_TEXTURE_RECT : PIPE_TEXTURE_2D)) :
			(dims == 3) ? PIPE_TEXTURE_2D :
			PIPE_MAX_TEXTURE_TYPES,
			1,
			PIPE_BIND_SAMPLER_VIEW);

  if(pdstformat == PIPE_FORMAT_NONE) {
    RSXGL_ERROR(GL_INVALID_VALUE,false);
  }

  if(width < 0 || height < 0 || depth < 0 ||
     width > RSXGL_MAX_TEXTURE_SIZE || height > RSXGL_MAX_TEXTURE_SIZE || depth > RSXGL_MAX_TEXTURE_SIZE) {
    RSXGL_ERROR(GL_INVALID_VALUE,false);
  }

#if 0
  // TODO: Orphan the texture
  if(texture.timestamp != 0 && (!rsxgl_timestamp_passed(ctx -> cached_timestamp,ctx -> timestamp_sync,texture.timestamp))) {
    texture.timestamp = 0;
  }
#else
  if(texture.timestamp > 0) {
    rsxgl_timestamp_wait(ctx,texture.timestamp);
    texture.timestamp = 0;
  }
#endif

  // set the texture's invalid & allocated bits:
  texture.invalid = 1;
  texture.invalid_complete = 1;
  texture.complete = 0;
  texture.arena = ctx -> arena_binding.names[RSXGL_TEXTURE_ARENA];

  // set the mipmap level data:
  texture_t::level_t & level = texture.levels[_level];

  if(level.pformat != pdstformat || level.size[0] != width || level.size[1] != height || level.size[2] != depth) {
    rsxgl_texture_level_reset_storage(level);
    rsxgl_texture_level_format(level,dims,pdstformat,width,height,depth);
  }

  ctx -> invalid_textures |= texture.binding_bitfield;

  RSXGL_NOERROR(true);
}

static inline bool
rsxgl_tex_subimage_init(rsxgl_context_t * ctx,texture_t & texture,GLint _level,GLint x,GLint y,GLint z,GLsizei width,GLsizei height,GLsizei depth,
			pipe_format * pdstformat,uint32_t * dstpitch,void ** dstaddress,memory_t * dstmem)
{
  rsxgl_assert(width > 0);
  rsxgl_assert(height > 0);
  rsxgl_assert(depth > 0);

  if(_level < 0 || (boost::static_log2_argument_type)_level >= texture_t::max_levels) {
    RSXGL_ERROR(GL_INVALID_VALUE,false);
  }
  
  if(width < 0 || height < 0 || depth < 0 ||
     width > RSXGL_MAX_TEXTURE_SIZE || height > RSXGL_MAX_TEXTURE_SIZE || depth > RSXGL_MAX_TEXTURE_SIZE) {
    RSXGL_ERROR(GL_INVALID_VALUE,false);
  }

  // TODO: Asynchronous updating:
  if(texture.timestamp > 0) {
    rsxgl_timestamp_wait(ctx,texture.timestamp);
    texture.timestamp = 0;
  }

  texture_t::dimension_size_type size[3] = { 0,0,0 };
  *pdstformat = PIPE_FORMAT_NONE;
  *dstpitch = 0;

  // the texture's storage is allocated (either by rsxgl_tex_storage, or by having previously validated a texture specified with rsxgl_tex_image)
  if(texture.memory) {
    const uint32_t offset = rsxgl_get_tex_level_offset_size(texture.size,texture.pitch,_level,size);

    *pdstformat = texture.pformat;
    *dstpitch = texture.pitch;

    *dstaddress = rsxgl_arena_address(memory_arena_t::storage().at(texture.arena),texture.memory + offset);
    *dstmem = texture.memory + offset;
  }
  // texture hasn't been allocated, see if the texture level was specified:
  else if(texture.levels[_level].pformat != PIPE_FORMAT_NONE) {
    texture_t::level_t & level = texture.levels[_level];

    if(!level.memory) {
      rsxgl_texture_level_validate_storage(level);
    }

    size[0] = level.size[0];
    size[1] = level.size[1];
    size[2] = level.size[2];
    
    *pdstformat = level.pformat;
    *dstpitch = level.pitch;

    *dstaddress = rsxgl_texture_migrate_address(level.memory.offset);
    *dstmem = level.memory;
  }
  // rsxgl_tex_image for this level was never called. fail:
  else {
    *dstaddress = 0;
    *dstmem = memory_t();

    RSXGL_ERROR(GL_INVALID_OPERATION,false);
  }

  if((x + width) > size[0] || (y + height) > size[1] || (z + depth) > size[2]) {
    RSXGL_ERROR(GL_INVALID_VALUE,false);
  }

  RSXGL_NOERROR(true);
}

static inline void
rsxgl_tex_image(rsxgl_context_t * ctx,texture_t & texture,const uint8_t dims,const bool cube,const bool rect,GLint _level,GLint glinternalformat,GLsizei width,GLsizei height,GLsizei depth,
		GLenum format,GLenum type,const GLvoid * data)
{
  const bool result = rsxgl_tex_image_format(ctx,texture,dims,cube,rect,_level,glinternalformat,width,height,depth);

  if(result) {
    const pipe_format psrcformat = rsxgl_choose_source_format(format,type);

    if(psrcformat == PIPE_FORMAT_NONE) {
      RSXGL_ERROR_(GL_INVALID_VALUE);
    }

    if(ctx -> buffer_binding.names[RSXGL_PIXEL_UNPACK_BUFFER] != 0 ||
       data != 0) {
      texture_t::level_t & level = texture.levels[_level];

      if(!level.memory) {
	rsxgl_texture_level_validate_storage(level);
      }

      rsxgl_assert(level.memory);

      if(ctx -> buffer_binding.names[RSXGL_PIXEL_UNPACK_BUFFER] != 0) {
	const buffer_t & srcbuffer = ctx -> buffer_binding[RSXGL_PIXEL_UNPACK_BUFFER];
	const memory_t & srcmem = srcbuffer.memory + rsxgl_pointer_to_offset(data);

	rsxgl_util_format_translate_dma(ctx,
					level.pformat,
					rsxgl_texture_migrate_address(level.memory.offset),level.memory,level.pitch,0,0,
					psrcformat,
					rsxgl_arena_address(memory_arena_t::storage().at(srcbuffer.arena),srcmem),srcmem,util_format_get_stride(psrcformat,width),0,0,
					width,height);
      }
      else if(data != 0) {
	util_format_translate(level.pformat,rsxgl_texture_migrate_address(level.memory.offset),level.pitch,0,0,
			      psrcformat,data,util_format_get_stride(psrcformat,width),0,0,width,height);
      }
    }
  }
}

static inline void
rsxgl_tex_subimage(rsxgl_context_t * ctx,texture_t & texture,GLint _level,GLint x,GLint y,GLint z,GLsizei width,GLsizei height,GLsizei depth,
		   GLenum format,GLenum type,const GLvoid * data)
{
  pipe_format pdstformat = PIPE_FORMAT_NONE;
  uint32_t dstpitch = 0;
  void * dstaddress = 0;
  memory_t dstmem;
  const bool result = rsxgl_tex_subimage_init(ctx,texture,_level,x,y,z,width,height,depth,&pdstformat,&dstpitch,&dstaddress,&dstmem);

  if(result) {
    // pick a format:
    const pipe_format psrcformat = rsxgl_choose_source_format(format,type);

    if(psrcformat == PIPE_FORMAT_NONE) {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }

    rsxgl_assert(dstaddress != 0);
    rsxgl_assert(dstmem);

    if(ctx -> buffer_binding.names[RSXGL_PIXEL_UNPACK_BUFFER] != 0) {
      const buffer_t & srcbuffer = ctx -> buffer_binding[RSXGL_PIXEL_UNPACK_BUFFER];
      const memory_t & srcmem = srcbuffer.memory + rsxgl_pointer_to_offset(data);

      rsxgl_util_format_translate_dma(ctx,
				      pdstformat,
				      dstaddress,dstmem,dstpitch,x,y,
				      psrcformat,
				      rsxgl_arena_address(memory_arena_t::storage().at(srcbuffer.arena),srcmem),srcmem,util_format_get_stride(psrcformat,width),0,0,
				      width,height);
    }
    else if(data) {
      rsxgl_assert(dstaddress != 0);

      util_format_translate(pdstformat,dstaddress,dstpitch,x,y,
			    psrcformat,data,util_format_get_stride(psrcformat,width),0,0,width,height);
    }

    RSXGL_NOERROR_();
  }
}

static inline void
rsxgl_copy_tex_image(rsxgl_context_t * ctx,texture_t & texture,const uint8_t dims,const bool cube,const bool rect,GLint _level,GLint glinternalformat,GLint x,GLint y,GLsizei width,GLsizei height)
{
  const bool result = rsxgl_tex_image_format(ctx,texture,dims,cube,rect,_level,glinternalformat,width,height,1);

  if(result) {
    const uint32_t timestamp = rsxgl_timestamp_create(ctx,1);
    
    framebuffer_t & framebuffer = ctx -> framebuffer_binding[RSXGL_READ_FRAMEBUFFER];
    rsxgl_framebuffer_validate(ctx,framebuffer,timestamp);

    if(framebuffer.color_pformat != PIPE_FORMAT_NONE && framebuffer.read_surface.memory) {
      texture_t::level_t & level = texture.levels[_level];
      if(!level.memory) {
	rsxgl_texture_level_validate_storage(level);
      }

      rsxgl_assert(level.memory);

      rsxgl_util_format_translate_dma(ctx,
				      level.pformat,
				      rsxgl_texture_migrate_address(level.memory.offset),level.memory,level.pitch,0,0,
				      framebuffer.color_pformat,
				      framebuffer.read_address,framebuffer.read_surface.memory,framebuffer.read_surface.pitch,
				      std::min((unsigned)x,(unsigned)framebuffer.size[0] - 1),std::min((unsigned)y,(unsigned)framebuffer.size[1] - 1),
				      std::min((unsigned)width,(unsigned)framebuffer.size[0] - x),std::min((unsigned)height,(unsigned)framebuffer.size[1] - y));
    }

    rsxgl_timestamp_post(ctx,timestamp);
  }
  else {
    rsxgl_debug_printf("%s: format failed\n",__PRETTY_FUNCTION__);
  }
}

static inline void
rsxgl_copy_tex_subimage(rsxgl_context_t * ctx,texture_t & texture,GLint _level,GLint xoffset,GLint yoffset,GLint zoffset,GLint x,GLint y,GLsizei width,GLsizei height)
{
  pipe_format pdstformat = PIPE_FORMAT_NONE;
  uint32_t dstpitch = 0;
  void * dstaddress = 0;
  memory_t dstmem;
  const bool result = rsxgl_tex_subimage_init(ctx,texture,_level,xoffset,yoffset,zoffset,width,height,1,&pdstformat,&dstpitch,&dstaddress,&dstmem);

  if(result) {
    const uint32_t timestamp = rsxgl_timestamp_create(ctx,1);
    
    framebuffer_t & framebuffer = ctx -> framebuffer_binding[RSXGL_READ_FRAMEBUFFER];
    rsxgl_framebuffer_validate(ctx,framebuffer,timestamp);

    if(framebuffer.color_pformat != PIPE_FORMAT_NONE && framebuffer.read_surface.memory) {
      rsxgl_assert(dstaddress != 0);
      rsxgl_assert(dstmem);

      rsxgl_util_format_translate_dma(ctx,
				      pdstformat,
				      dstaddress,dstmem,dstpitch,0,0,
				      framebuffer.color_pformat,
				      framebuffer.read_address,framebuffer.read_surface.memory,framebuffer.read_surface.pitch,
				      std::min((unsigned)x,(unsigned)framebuffer.size[0] - 1),std::min((unsigned)y,(unsigned)framebuffer.size[1] - 1),
				      std::min((unsigned)width,(unsigned)framebuffer.size[0] - x),std::min((unsigned)height,(unsigned)framebuffer.size[1] - y));
    }
    
    rsxgl_timestamp_post(ctx,timestamp);
  }
}

GLAPI void APIENTRY
glTexImage1D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
  if(!(target == GL_TEXTURE_1D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_tex_image(ctx,texture,1,false,false,level,internalformat,std::max(width,1),1,1,format,type,pixels);
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

  rsxgl_tex_image(ctx,texture,2,target == GL_TEXTURE_CUBE_MAP, target == GL_TEXTURE_RECTANGLE,level,internalformat,std::max(width,1),std::max(height,1),1,format,type,pixels);
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

  rsxgl_tex_image(ctx,texture,3,false,false,level,internalformat,std::max(width,1),std::max(height,1),std::max(depth,1),format,type,pixels);
}

GLAPI void APIENTRY
glTexStorage1D(GLenum target, GLsizei levels,GLenum internalformat,GLsizei width)
{
  if(!(target == GL_TEXTURE_1D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_tex_storage(ctx,texture,1,false,false,levels,internalformat,std::max(width,1),1,1);
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

  rsxgl_tex_storage(ctx,texture,2,target == GL_TEXTURE_CUBE_MAP,target == GL_TEXTURE_RECTANGLE,levels,internalformat,std::max(width,1),std::max(height,1),1);
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

  rsxgl_tex_storage(ctx,texture,3,false,false,levels,internalformat,std::max(width,1),std::max(height,1),std::max(depth,1));
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
  if(!(target == GL_TEXTURE_1D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_copy_tex_image(ctx,texture,1,false,false,level,internalformat,x,y,width,1);
}

GLAPI void APIENTRY
glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
  if(!(target == GL_TEXTURE_2D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_copy_tex_image(ctx,texture,2,false,false,level,internalformat,x,y,width,height);
}

GLAPI void APIENTRY
glCopyTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
  if(!(target == GL_TEXTURE_1D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_copy_tex_subimage(ctx,texture,level,xoffset,0,0,x,y,width,1);
}

GLAPI void APIENTRY
glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
  if(!(target == GL_TEXTURE_2D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_copy_tex_subimage(ctx,texture,level,xoffset,yoffset,0,x,y,width,height);
}

GLAPI void APIENTRY
glCopyTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
  if(!(target == GL_TEXTURE_3D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  texture_t & texture = ctx -> texture_binding[ctx -> active_texture];

  rsxgl_copy_tex_subimage(ctx,texture,level,xoffset,yoffset,zoffset,x,y,width,height);
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
rsxgl_texture_validate(rsxgl_context_t * ctx,texture_t & texture,const uint32_t timestamp)
{
  rsxgl_assert(timestamp >= texture.timestamp);
  texture.timestamp = timestamp;

  if(texture.invalid) {
    rsxgl_texture_reset_storage(texture);
    rsxgl_texture_validate_complete(ctx,texture);

    if(texture.complete) {
      rsxgl_texture_validate_storage(ctx,texture);

      if(texture.memory) {
	const pipe_format pdstformat = texture.pformat;
	texture_t::dimension_size_type size[3] = { texture.size[0], texture.size[1], texture.size[2] };
	const uint32_t dstpitch = texture.pitch;
	uint32_t dstoffset = 0;
	unsigned int ndelete = 0;

	// transfer contents of each mipmap level:
	{
	  texture_t::level_t * plevel = texture.levels;
	  for(texture_t::level_size_type i = 0,n = texture.num_levels;i < n;++i,++plevel) {
	    if(plevel -> memory) {
#if 0
	      rsxgl_debug_printf("\tcopying mipmap level:%u pformat:%u pitch:%u size:%ux%ux%u from:%u pdstformat:%u dstpitch:%u to:%u\n",
				 (unsigned int)i,(unsigned int)plevel -> pformat,(unsigned int)plevel -> pitch,
				 (unsigned int)std::min(size[0],plevel -> size[0]),(unsigned int)std::min(size[1],plevel -> size[1]),(unsigned int)std::min(size[2],plevel -> size[2]),
				 (unsigned long)plevel -> memory.offset,
				 
				 (unsigned int)pdstformat,(unsigned int)dstpitch,
				 (unsigned long)texture.memory.offset);
#endif
	      
#if 0
	      rsxgl_texture_blit(ctx,
				 pdstformat,texture.memory + dstoffset,dstpitch,0,0,
				 plevel -> pformat,plevel -> memory,plevel -> pitch,0,0,
				 std::min(size[0],plevel -> size[0]),std::min(size[1],plevel -> size[1]));
#endif

	      const memory_t dstmem = texture.memory + dstoffset;

	      rsxgl_util_format_translate_dma(ctx,
					      pdstformat,
					      rsxgl_arena_address(memory_arena_t::storage().at(texture.arena),dstmem),dstmem,dstpitch,0,0,
					      plevel -> pformat,
					      rsxgl_texture_migrate_address(plevel -> memory.offset),plevel -> memory,plevel -> pitch,0,0,
					      std::min(size[0],plevel -> size[0]),std::min(size[1],plevel -> size[1]));

	      if(plevel -> memory.owner) {
		++ndelete;
	      }
	    }
	    
	    dstoffset += dstpitch * size[1] * size[2];
	    for(int j = 0;j < 3;++j) {
	      size[j] = std::max(size[j] >> 1,1);
	    }
	  }
	}

	// TODO: wait for transfers to complete, then delete memory:
	if(ndelete) {
	  texture_t::level_t * plevel = texture.levels;
	  for(texture_t::level_size_type i = 0,n = texture.num_levels;i < n;++i,++plevel) {
	    if(plevel -> memory.owner) {
	      // rsxgl_texture_level_reset_storage(*plevel);
	    }
	  }
	}
      }
    }

    texture.invalid = 0;
  }
}

void
rsxgl_textures_validate(rsxgl_context_t * ctx,program_t & program,const uint32_t timestamp)
{
  gcmContextData * context = ctx -> base.gcm_context;

  // Invalidate the texture cache.
  // TODO: determine when this is necessary to do, and only do it then.
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

  const program_t::texture_assignments_bitfield_type
    textures_enabled = program.textures_enabled,
    invalid_texture_assignments = ctx -> invalid_texture_assignments;
  const program_t::texture_assignments_type
    texture_assignments = program.texture_assignments;

  program_t::texture_assignments_bitfield_type::const_iterator
    enabled_it = textures_enabled.begin(),
    invalid_it = invalid_texture_assignments.begin();
  program_t::texture_assignments_type::const_iterator
    assignment_it = texture_assignments.begin();

  const bit_set< RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS >
    invalid_textures = ctx -> invalid_textures,
    invalid_samplers = ctx -> invalid_samplers;

  bit_set< RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS >
    validated;

  // Vertex program textures:
  for(program_t::texture_size_type index = 0;index < RSXGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS;++index,enabled_it.next(textures_enabled),invalid_it.next(invalid_texture_assignments),assignment_it.next(texture_assignments)) {
    if(!enabled_it.test()) continue;

    const texture_t::binding_type::size_type api_index = assignment_it.value();

    if(ctx -> texture_binding.names[api_index] != 0) {
      texture_t & texture = ctx -> texture_binding[api_index];
      rsxgl_assert(timestamp >= texture.timestamp);
      texture.timestamp = timestamp;
    }

    if(invalid_it.test() || invalid_samplers.test(api_index)) {
      validated.set(api_index);

      // TODO: Set LOD min, max, bias:
    }
    if(invalid_it.test() || invalid_textures.test(api_index)) {
      texture_t & texture = ctx -> texture_binding[api_index];

      rsxgl_texture_validate(ctx,texture,timestamp);
      
      if(texture.memory) {
	const uint32_t format = texture.format & (0x3 | NV30_3D_TEX_FORMAT_DIMS__MASK | NV30_3D_TEX_FORMAT_FORMAT__MASK | NV40_3D_TEX_FORMAT_MIPMAP_COUNT__MASK);
	const uint32_t format_format = (format & NV30_3D_TEX_FORMAT_FORMAT__MASK);

	static const uint32_t
	  RGBA32F_format = NV40_3D_TEX_FORMAT_FORMAT_RGBA32F | NV40_3D_TEX_FORMAT_LINEAR | 0x9000,
	  R32F_format = 0x1b00 | NV40_3D_TEX_FORMAT_LINEAR | 0x9000;

	if(format_format == RGBA32F_format || format_format == R32F_format) {
	  // activate the texture:
	  uint32_t * buffer = gcm_reserve(context,9);

#define NVFX_VERTEX_TEX_OFFSET(INDEX) (0x00000900 + 0x20 * (INDEX))
	  gcm_emit_method(&buffer,NVFX_VERTEX_TEX_OFFSET(index),2);
	  gcm_emit(&buffer,texture.memory.offset);
	  gcm_emit(&buffer,format);
	  
#define NVFX_VERTEX_TEX_ENABLE(INDEX) (0x0000090c + 0x20 * (INDEX))
	  gcm_emit_method(&buffer,NVFX_VERTEX_TEX_ENABLE(index),1);
	  gcm_emit(&buffer,NV40_3D_TEX_ENABLE_ENABLE);
	  
#define NVFX_VERTEX_TEX_NPOT_SIZE(INDEX) (0x00000918 + 0x20 * (INDEX))
	  gcm_emit_method(&buffer,NVFX_VERTEX_TEX_NPOT_SIZE(index),1);
	  gcm_emit(&buffer,((uint32_t)texture.size[0] << NV30_3D_TEX_NPOT_SIZE_W__SHIFT) | (uint32_t)texture.size[1]);
	
#define NVFX_VERTEX_TEX_SIZE1(INDEX) (0x00000910 + 0x20 * (INDEX))
	  gcm_emit_method(&buffer,NVFX_VERTEX_TEX_SIZE1(index),1);
	  gcm_emit(&buffer,(uint32_t)texture.pitch);
	  
	  gcm_finish_commands(context,&buffer);
	}
	else {
	  uint32_t * buffer = gcm_reserve(context,2);

	  gcm_emit_method(&buffer,NVFX_VERTEX_TEX_ENABLE(index),1);
	  gcm_emit(&buffer,0);

	  gcm_finish_commands(context,&buffer);
	}
      }

      validated.set(api_index);
    }
  }

  // Fragment program textures:
  for(program_t::texture_size_type index = 0;index < RSXGL_MAX_TEXTURE_IMAGE_UNITS;++index,enabled_it.next(textures_enabled),invalid_it.next(invalid_texture_assignments),assignment_it.next(texture_assignments)) {
    if(!enabled_it.test()) continue;

    const texture_t::binding_type::size_type api_index = assignment_it.value();

    if(ctx -> texture_binding.names[api_index] != 0) {
      texture_t & texture = ctx -> texture_binding[api_index];
      rsxgl_assert(timestamp >= texture.timestamp);
      texture.timestamp = timestamp;
    }

    if(invalid_it.test() || invalid_samplers.test(api_index)) {
      const sampler_t & sampler = (ctx -> sampler_binding.names[api_index] != 0) ? ctx -> sampler_binding[api_index] : ctx -> texture_binding[api_index].sampler;
      
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
      
      gcm_emit_method(&buffer,NV30_3D_TEX_FILTER(index),1);
      gcm_emit(&buffer,filter);
      
      gcm_emit_method(&buffer,NV30_3D_TEX_WRAP(index),1);
      gcm_emit(&buffer,wrap | compare);

      // TODO: Set LOD min, max, bias:
      
      gcm_finish_commands(context,&buffer);

      validated.set(api_index);
    }
    if(invalid_it.test() || invalid_textures.test(api_index)) {
      texture_t & texture = ctx -> texture_binding[api_index];

      rsxgl_texture_validate(ctx,texture,timestamp);
      
      if(texture.memory) {
	// activate the texture:
	uint32_t * buffer = gcm_reserve(context,11);
	
	gcm_emit_method(&buffer,NV30_3D_TEX_OFFSET(index),2);
	gcm_emit(&buffer,texture.memory.offset);
	gcm_emit(&buffer,texture.format);
	
	gcm_emit_method(&buffer,NV30_3D_TEX_ENABLE(index),1);
	gcm_emit(&buffer,NV40_3D_TEX_ENABLE_ENABLE);
	
	gcm_emit_method(&buffer,NV30_3D_TEX_NPOT_SIZE(index),1);
	gcm_emit(&buffer,((uint32_t)texture.size[0] << NV30_3D_TEX_NPOT_SIZE_W__SHIFT) | (uint32_t)texture.size[1]);
	
	gcm_emit_method(&buffer,NV40_3D_TEX_SIZE1(index),1);
	gcm_emit(&buffer,((uint32_t)texture.size[2] << NV40_3D_TEX_SIZE1_DEPTH__SHIFT) | (uint32_t)texture.pitch);

	gcm_emit_method(&buffer,NV30_3D_TEX_SWIZZLE(index),1);
	gcm_emit(&buffer,texture.remap);
	
	gcm_finish_commands(context,&buffer);
      }

      validated.set(api_index);
    }
  }

  ctx -> invalid_texture_assignments.reset();
  ctx -> invalid_samplers &= ~validated;
  ctx -> invalid_textures &= ~validated;
}
