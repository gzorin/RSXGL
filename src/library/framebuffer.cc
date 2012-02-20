// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// framebuffer.cc - Offscreen rendering targets.

#include "rsxgl_context.h"
#include "gl_constants.h"
#include "framebuffer.h"

#include <GL3/gl3.h>
#include "error.h"

#include "gcm.h"
#include "nv40.h"
#include "gl_fifo.h"
#include "cxxutil.h"

#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "util/u_format.h"

extern "C" {
  enum pipe_format
  rsxgl_choose_format(struct pipe_screen *screen, GLenum internalFormat,
		      GLenum format, GLenum type,
		      enum pipe_texture_target target, unsigned sample_count,
		      unsigned bindings);
  int
  rsxgl_get_format_color_bit_depth(enum pipe_format pformat,int channel);
  
  int
  rsxgl_get_format_depth_bit_depth(enum pipe_format pformat,int channel);

  uint32_t
  nvfx_get_framebuffer_format(enum pipe_format cformat,enum pipe_format zformat);
}

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

// Renderbuffers:
renderbuffer_t::storage_type & renderbuffer_t::storage()
{
  return current_object_ctx() -> renderbuffer_storage();
}

renderbuffer_t::renderbuffer_t()
  : glformat(GL_NONE), deleted(0), timestamp(0), ref_count(0), arena(0), pformat(PIPE_FORMAT_NONE), samples(0)
{
  size[0] = 0;
  size[1] = 0;
}

renderbuffer_t::~renderbuffer_t()
{
  // Free memory used by this buffer:
  if(surface.memory.offset != 0) {
    rsxgl_arena_free(memory_arena_t::storage().at(arena),surface.memory);
  }
}

GLAPI void APIENTRY
glGenRenderbuffers (GLsizei count, GLuint *renderbuffers)
{
  GLsizei n = renderbuffer_t::storage().create_names(count,renderbuffers);

  if(count != n) {
    RSXGL_ERROR_(GL_OUT_OF_MEMORY);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDeleteRenderbuffers (GLsizei count, const GLuint *renderbuffers)
{
  struct rsxgl_context_t * ctx = current_ctx();

  for(GLsizei i = 0;i < count;++i,++renderbuffers) {
    GLuint renderbuffer_name = *renderbuffers;

    if(renderbuffer_name == 0) continue;

    // Free resources used by this object:
    if(renderbuffer_t::storage().is_object(renderbuffer_name)) {
      ctx -> renderbuffer_binding.unbind_from_all(renderbuffer_name);

      // TODO - orphan it, instead of doing this:
      renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(renderbuffer_name);
      if(renderbuffer.timestamp > 0) {
	rsxgl_timestamp_wait(ctx,renderbuffer.timestamp);
	renderbuffer.timestamp = 0;
      }

      renderbuffer_t::gl_object_type::maybe_delete(renderbuffer_name);
    }
    else if(renderbuffer_t::storage().is_name(renderbuffer_name)) {
      renderbuffer_t::storage().destroy(renderbuffer_name);
    }
  }

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsRenderbuffer (GLuint renderbuffer)
{
  return renderbuffer_t::storage().is_object(renderbuffer);
}

static inline uint32_t
rsxgl_renderbuffer_target(GLenum target)
{
  switch(target) {
  case GL_RENDERBUFFER:
    return RSXGL_RENDERBUFFER;
    break;
  default:
    return RSXGL_MAX_RENDERBUFFER_TARGETS;
  };
}

GLAPI void APIENTRY
glBindRenderbuffer (GLuint target, GLuint renderbuffer_name)
{
  const uint32_t rsx_target = rsxgl_renderbuffer_target(target);
  if(rsx_target == RSXGL_MAX_RENDERBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!(renderbuffer_name == 0 || renderbuffer_t::storage().is_name(renderbuffer_name))) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(renderbuffer_name != 0 && !renderbuffer_t::storage().is_object(renderbuffer_name)) {
    renderbuffer_t::storage().create_object(renderbuffer_name);
  }

  rsxgl_context_t * ctx = current_ctx();
  ctx -> renderbuffer_binding.bind(rsx_target,renderbuffer_name);

  RSXGL_NOERROR_();
}

static inline void
rsxgl_renderbuffer_storage(rsxgl_context_t * ctx,const renderbuffer_t::name_type renderbuffer_name,GLenum glinternalformat, GLsizei width, GLsizei height)
{
  if(width < 0 || width > RSXGL_MAX_RENDERBUFFER_SIZE || height < 0 || height > RSXGL_MAX_RENDERBUFFER_SIZE) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const pipe_format pformat = rsxgl_choose_format(ctx -> screen(),
						  glinternalformat,GL_NONE,GL_NONE,
						  PIPE_TEXTURE_RECT,
						  1,
						  PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET);

  if(pformat == PIPE_FORMAT_NONE) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(renderbuffer_name);

  // TODO - orphan instead of delete:
  if(renderbuffer.timestamp > 0) {
    rsxgl_timestamp_wait(ctx,renderbuffer.timestamp);
    renderbuffer.timestamp = 0;
  }

  surface_t & surface = renderbuffer.surface;

  if(surface.memory.offset != 0) {
    rsxgl_arena_free(memory_arena_t::storage().at(renderbuffer.arena),surface.memory);
  }

  memory_arena_t::name_type arena = ctx -> arena_binding.names[RSXGL_RENDERBUFFER_ARENA];

  const uint32_t pitch = align_pot< uint32_t, 64 >(util_format_get_stride(pformat,width));
  const uint32_t nbytes = util_format_get_2d_size(pformat,pitch,height);

  surface.memory = rsxgl_arena_allocate(memory_arena_t::storage().at(arena),128,nbytes);
  if(surface.memory.offset == 0) {
    RSXGL_ERROR_(GL_OUT_OF_MEMORY);
  }

  renderbuffer.glformat = glinternalformat;
  renderbuffer.pformat = pformat;
  renderbuffer.size[0] = width;
  renderbuffer.size[1] = height;
  surface.pitch = pitch;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glRenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
  const uint32_t rsx_target = rsxgl_renderbuffer_target(target);
  if(rsx_target == RSXGL_MAX_RENDERBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> renderbuffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_renderbuffer_storage(ctx,ctx -> renderbuffer_binding.names[rsx_target],internalformat,width,height);
}

GLAPI void APIENTRY
glRenderbufferStorageMultisample (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
  const uint32_t rsx_target = rsxgl_renderbuffer_target(target);
  if(rsx_target == RSXGL_MAX_RENDERBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(samples > RSXGL_MAX_SAMPLES) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> renderbuffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_renderbuffer_storage(ctx,ctx -> renderbuffer_binding.names[rsx_target],internalformat,width,height);
}

static inline void
rsxgl_get_renderbuffer_parameteriv(rsxgl_context_t * ctx,const renderbuffer_t::name_type renderbuffer_name, GLenum pname, GLint *params)
{
  const renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(renderbuffer_name);

  if(pname == GL_RENDERBUFFER_WIDTH) {
    *params = renderbuffer.size[0];
  }
  else if(pname == GL_RENDERBUFFER_HEIGHT) {
    *params = renderbuffer.size[1];
  }
  else if(pname == GL_RENDERBUFFER_INTERNAL_FORMAT) {
    *params = renderbuffer.glformat;
  }
  else if(pname == GL_RENDERBUFFER_SAMPLES) {
    *params = renderbuffer.samples;
  }
  else if(pname == GL_RENDERBUFFER_RED_SIZE) {
    *params = rsxgl_get_format_color_bit_depth(renderbuffer.pformat,0);
  }
  else if(pname == GL_RENDERBUFFER_BLUE_SIZE) {
    *params = rsxgl_get_format_color_bit_depth(renderbuffer.pformat,1);
  }
  else if(pname == GL_RENDERBUFFER_GREEN_SIZE) {
    *params = rsxgl_get_format_color_bit_depth(renderbuffer.pformat,2);
  }
  else if(pname == GL_RENDERBUFFER_ALPHA_SIZE) {
    *params = rsxgl_get_format_color_bit_depth(renderbuffer.pformat,3);
  }
  else if(pname == GL_RENDERBUFFER_DEPTH_SIZE) {
    *params = rsxgl_get_format_depth_bit_depth(renderbuffer.pformat,0);
  }
  else if(pname == GL_RENDERBUFFER_STENCIL_SIZE) {
    *params = rsxgl_get_format_depth_bit_depth(renderbuffer.pformat,1);
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetRenderbufferParameteriv (GLenum target, GLenum pname, GLint *params)
{
  const uint32_t rsx_target = rsxgl_renderbuffer_target(target);
  if(rsx_target == RSXGL_MAX_RENDERBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> renderbuffer_binding.names[rsx_target] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_get_renderbuffer_parameteriv(ctx,ctx -> renderbuffer_binding.names[rsx_target],pname,params);
}

// Framebuffers:
framebuffer_t::storage_type & framebuffer_t::storage()
{
  return current_object_ctx() -> framebuffer_storage();
}

framebuffer_t::framebuffer_t()
  : is_default(0), invalid(0), format(0), enabled(0)
{
  for(size_t i = 0;i < RSXGL_MAX_ATTACHMENTS;++i) {
    attachment_types.set(i,RSXGL_ATTACHMENT_TYPE_NONE);
    attachments[i] = 0;
    attachment_layers[i] = 0;
    attachment_levels[i] = 0;
  }

  size[0] = 0;
  size[1] = 0;

  mapping.set(0,0);
  for(size_t i = 1;i < RSXGL_MAX_DRAW_BUFFERS;++i) {
    mapping.set(i,RSXGL_MAX_COLOR_ATTACHMENTS);
  }

  write_mask.all = 0;
}

framebuffer_t::~framebuffer_t()
{
  for(size_t i = 0;i < RSXGL_MAX_ATTACHMENTS;++i) {
    const uint32_t attachment_type = attachment_types.get(i);
    if(attachment_type == RSXGL_ATTACHMENT_TYPE_NONE) {
      continue;
    }
    else if(attachment_type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER && attachments[i] != 0) {
      renderbuffer_t::gl_object_type::unref_and_maybe_delete(attachments[i]);
    }
    else if(attachment_type == RSXGL_ATTACHMENT_TYPE_TEXTURE && attachments[i] != 0) {
      texture_t::gl_object_type::unref_and_maybe_delete(attachments[i]);
    }
  }
}

static inline void
rsxgl_framebuffer_detach(framebuffer_t & framebuffer,const uint32_t rsx_attachment)
{
  if(framebuffer.attachments[rsx_attachment] != 0) {
    const uint32_t attachment_type = framebuffer.attachment_types.get(rsx_attachment);
    if(attachment_type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
      // TODO - maybe orphan:
      renderbuffer_t::gl_object_type::unref_and_maybe_delete(framebuffer.attachments[rsx_attachment]);
    }
    else if(attachment_type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
      // TODO - maybe orphan:
      texture_t::gl_object_type::unref_and_maybe_delete(framebuffer.attachments[rsx_attachment]);
    }
    framebuffer.attachment_types.set(rsx_attachment,RSXGL_ATTACHMENT_TYPE_NONE);
    framebuffer.attachments[rsx_attachment] = 0;
    framebuffer.attachment_layers[rsx_attachment] = 0;
    framebuffer.attachment_levels[rsx_attachment] = 0;
  }
}

static inline void
rsxgl_framebuffer_invalidate(rsxgl_context_t * ctx,const framebuffer_t::name_type framebuffer_name,framebuffer_t & framebuffer)
{
  framebuffer.invalid = 1;

  if(ctx -> framebuffer_binding.is_bound(RSXGL_DRAW_FRAMEBUFFER,framebuffer_name)) {
    ctx -> invalid.parts.draw_framebuffer = 1;
    ctx -> state.invalid.parts.write_mask = 1;
  }
  if(ctx -> framebuffer_binding.is_bound(RSXGL_READ_FRAMEBUFFER,framebuffer_name)) {
    ctx -> invalid.parts.read_framebuffer = 1;
  }
}

static inline uint32_t
rsxgl_framebuffer_target(GLenum target)
{
  switch(target) {
  case GL_FRAMEBUFFER:
  case GL_DRAW_FRAMEBUFFER:
    return RSXGL_DRAW_FRAMEBUFFER;
    break;
  case GL_READ_FRAMEBUFFER:
    return RSXGL_READ_FRAMEBUFFER;
    break;
  default:
    return RSXGL_MAX_FRAMEBUFFER_TARGETS;
  };
}

static inline uint32_t
rsxgl_framebuffer_attachment(const GLenum attachment)
{
  if(attachment >= GL_COLOR_ATTACHMENT0 && attachment < (GL_COLOR_ATTACHMENT0 + RSXGL_MAX_COLOR_ATTACHMENTS)) {
    return RSXGL_COLOR_ATTACHMENT0 + (attachment - GL_COLOR_ATTACHMENT0);
  }
  else if(attachment == GL_DEPTH_ATTACHMENT || attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
    return RSXGL_DEPTH_STENCIL_ATTACHMENT;
  }
  else {
    return RSXGL_MAX_ATTACHMENTS;
  }
}

static inline uint32_t
rsxgl_framebuffer_attachment_type(const GLenum target)
{
  if(target == GL_RENDERBUFFER) {
    return RSXGL_ATTACHMENT_TYPE_RENDERBUFFER;
  }
  else {
    return RSXGL_MAX_ATTACHMENT_TYPES;
  }
}

static inline void
rsxgl_framebuffer_renderbuffer(rsxgl_context_t * ctx,const framebuffer_t::name_type framebuffer_name,const GLenum attachment, const GLenum renderbuffertarget, const GLuint renderbuffer_name)
{
  const uint32_t rsx_attachment = rsxgl_framebuffer_attachment(attachment);
  if(rsx_attachment == RSXGL_MAX_ATTACHMENTS) {
    RSXGL_NOERROR_();
  }

  const uint32_t rsx_attachment_type = rsxgl_framebuffer_attachment_type(renderbuffertarget);
  if(rsx_attachment_type != RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(renderbuffer_name != 0 && !renderbuffer_t::storage().is_object(renderbuffer_name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  framebuffer_t & framebuffer = framebuffer_t::storage().at(framebuffer_name);

  // Detach whatever was there:
  rsxgl_framebuffer_detach(framebuffer,rsx_attachment);

  // Attach the renderbuffer to it:
  if(renderbuffer_name != 0) {
    framebuffer.attachment_types.set(rsx_attachment,RSXGL_ATTACHMENT_TYPE_RENDERBUFFER);
    framebuffer.attachments[rsx_attachment] = renderbuffer_name;
  }

  rsxgl_framebuffer_invalidate(ctx,framebuffer_name,framebuffer);

  RSXGL_NOERROR_();
}

static inline void
rsxgl_framebuffer_texture(rsxgl_context_t * ctx,const framebuffer_t::name_type framebuffer_name,const GLenum attachment,const uint8_t dims,const texture_t::name_type texture_name,const texture_t::dimension_size_type layer,const texture_t::level_size_type level)
{
  const uint32_t rsx_attachment = rsxgl_framebuffer_attachment(attachment);
  if(rsx_attachment == RSXGL_MAX_ATTACHMENTS) {
    RSXGL_NOERROR_();
  }

  if(texture_name != 0 && !texture_t::storage().is_object(texture_name)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(texture_name != 0 && (layer < 0 || level < 0)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(dims != 0 && dims != texture_t::storage().at(texture_name).dims) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  framebuffer_t & framebuffer = framebuffer_t::storage().at(framebuffer_name);

  // Detach whatever was there:
  rsxgl_framebuffer_detach(framebuffer,rsx_attachment);

  if(texture_name != 0) {
    framebuffer.attachment_types.set(rsx_attachment,RSXGL_ATTACHMENT_TYPE_TEXTURE);
    framebuffer.attachments[rsx_attachment] = texture_name;
    framebuffer.attachment_layers[rsx_attachment] = layer;
    framebuffer.attachment_levels[rsx_attachment] = level;
  }

  rsxgl_framebuffer_invalidate(ctx,framebuffer_name,framebuffer);
}

GLAPI void APIENTRY
glGenFramebuffers (GLsizei n, GLuint *framebuffers)
{
  GLsizei count = framebuffer_t::storage().create_names(n,framebuffers);

  if(count != n) {
    RSXGL_ERROR_(GL_OUT_OF_MEMORY);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDeleteFramebuffers (GLsizei n, const GLuint *framebuffers)
{
  struct rsxgl_context_t * ctx = current_ctx();

  for(GLsizei i = 0;i < n;++i,++framebuffers) {
    const GLuint framebuffer_name = *framebuffers;

    if(framebuffer_name == 0) continue;

    if(framebuffer_t::storage().is_object(framebuffer_name)) {
      ctx -> framebuffer_binding.unbind_from_all(framebuffer_name);
      framebuffer_t::storage().destroy(framebuffer_name);
    }
    else if(framebuffer_t::storage().is_name(framebuffer_name)) {
      framebuffer_t::storage().destroy(framebuffer_name);
    }
  }

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsFramebuffer (GLuint framebuffer)
{
  return framebuffer_t::storage().is_object(framebuffer);
}

GLAPI void APIENTRY
glBindFramebuffer (GLenum target, GLuint framebuffer_name)
{
  if(!(target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(!(framebuffer_name == 0 || framebuffer_t::storage().is_name(framebuffer_name))) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(framebuffer_name != 0 && !framebuffer_t::storage().is_object(framebuffer_name)) {
    framebuffer_t::storage().create_object(framebuffer_name);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
    ctx -> framebuffer_binding.bind(RSXGL_DRAW_FRAMEBUFFER,framebuffer_name);
    ctx -> invalid.parts.draw_framebuffer = 1;
    ctx -> state.invalid.parts.write_mask = 1;
  }
  if(target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) {
    ctx -> framebuffer_binding.bind(RSXGL_READ_FRAMEBUFFER,framebuffer_name);
    ctx -> invalid.parts.read_framebuffer = 1;
  }

  RSXGL_NOERROR_();
}

GLAPI GLenum APIENTRY
glCheckFramebufferStatus (GLenum target)
{
  //RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glFramebufferTexture1D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  const uint32_t rsx_framebuffer_target = rsxgl_framebuffer_target(target);
  if(rsx_framebuffer_target == RSXGL_MAX_FRAMEBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(texture != 0 && !(textarget == GL_TEXTURE_1D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  if(!ctx -> framebuffer_binding.is_anything_bound(rsx_framebuffer_target)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_framebuffer_texture(ctx,ctx -> framebuffer_binding.names[rsx_framebuffer_target],attachment,1,texture,0,level);
}

GLAPI void APIENTRY
glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  const uint32_t rsx_framebuffer_target = rsxgl_framebuffer_target(target);
  if(rsx_framebuffer_target == RSXGL_MAX_FRAMEBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(texture != 0 && !(textarget == GL_TEXTURE_2D ||
		       textarget == GL_TEXTURE_CUBE_MAP ||
		       textarget == GL_TEXTURE_RECTANGLE)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  if(!ctx -> framebuffer_binding.is_anything_bound(rsx_framebuffer_target)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_framebuffer_texture(ctx,ctx -> framebuffer_binding.names[rsx_framebuffer_target],attachment,2,texture,0,level);
}

GLAPI void APIENTRY
glFramebufferTexture3D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
  const uint32_t rsx_framebuffer_target = rsxgl_framebuffer_target(target);
  if(rsx_framebuffer_target == RSXGL_MAX_FRAMEBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(texture != 0 && !(textarget == GL_TEXTURE_3D)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  if(!ctx -> framebuffer_binding.is_anything_bound(rsx_framebuffer_target)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_framebuffer_texture(ctx,ctx -> framebuffer_binding.names[rsx_framebuffer_target],attachment,3,texture,zoffset,level);
}

GLAPI void APIENTRY
glFramebufferRenderbuffer (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
  const uint32_t rsx_framebuffer_target = rsxgl_framebuffer_target(target);
  if(rsx_framebuffer_target == RSXGL_MAX_FRAMEBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  if(!ctx -> framebuffer_binding.is_anything_bound(rsx_framebuffer_target)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_framebuffer_renderbuffer(ctx,ctx -> framebuffer_binding.names[rsx_framebuffer_target],attachment,renderbuffertarget,renderbuffer);
}

static inline void
rsxgl_get_framebuffer_attachment_parameteriv(rsxgl_context_t * ctx,const framebuffer_t::name_type framebuffer_name,GLenum attachment, GLenum pname, GLint *params)
{
  const framebuffer_t & framebuffer = framebuffer_t::storage().at(framebuffer_name);

  const uint32_t rsx_attachment = rsxgl_framebuffer_attachment(attachment);
  if(rsx_attachment == RSXGL_MAX_ATTACHMENTS) {
    RSXGL_NOERROR_();
  }

  const uint32_t attachment_type = framebuffer.attachment_types.get(rsx_attachment);

  if(pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) {
    if(attachment_type == RSXGL_ATTACHMENT_TYPE_NONE) {
      *params = GL_NONE;
    }
    else if(attachment_type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
      *params = GL_RENDERBUFFER;
    }
    else if(attachment_type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
      *params = GL_TEXTURE;
    }
  }
  else if(attachment_type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
    renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(framebuffer.attachments[rsx_attachment]);

    if(pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
      *params = framebuffer.attachments[rsx_attachment];
    }
    else if(pname == GL_RENDERBUFFER_RED_SIZE) { 
      *params = rsxgl_get_format_color_bit_depth(renderbuffer.pformat,0);
    }
    else if(pname == GL_RENDERBUFFER_BLUE_SIZE) {
      *params = rsxgl_get_format_color_bit_depth(renderbuffer.pformat,1);
    }
    else if(pname == GL_RENDERBUFFER_GREEN_SIZE) {
      *params = rsxgl_get_format_color_bit_depth(renderbuffer.pformat,2);
    }
    else if(pname == GL_RENDERBUFFER_ALPHA_SIZE) {
      *params = rsxgl_get_format_color_bit_depth(renderbuffer.pformat,3);
    }
    else if(pname == GL_RENDERBUFFER_DEPTH_SIZE) {
      *params = rsxgl_get_format_depth_bit_depth(renderbuffer.pformat,0);
    }
    else if(pname == GL_RENDERBUFFER_STENCIL_SIZE) {
      *params = rsxgl_get_format_depth_bit_depth(renderbuffer.pformat,1);
    }
    else if(pname == GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) {
      // TODO - I actually don't understand this
    }
    else if(pname == GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING) {
      // TODO
    }

    else {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else if(attachment_type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
    texture_t & texture = texture_t::storage().at(framebuffer.attachments[rsx_attachment]);

    if(pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
      *params = framebuffer.attachments[rsx_attachment];
    }
    else if(pname == GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL) {
      *params = framebuffer.attachment_levels[rsx_attachment];
    }
    else if(pname == GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE) {
      // TODO
    }
    else if(pname == GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER) {
      *params = framebuffer.attachment_layers[rsx_attachment];
    }
    else if(pname == GL_FRAMEBUFFER_ATTACHMENT_LAYERED) {
      // TODO
    }

    else if(pname == GL_RENDERBUFFER_RED_SIZE) { 
      *params = rsxgl_get_format_color_bit_depth(texture.pformat,0);
    }
    else if(pname == GL_RENDERBUFFER_BLUE_SIZE) {
      *params = rsxgl_get_format_color_bit_depth(texture.pformat,1);
    }
    else if(pname == GL_RENDERBUFFER_GREEN_SIZE) {
      *params = rsxgl_get_format_color_bit_depth(texture.pformat,2);
    }
    else if(pname == GL_RENDERBUFFER_ALPHA_SIZE) {
      *params = rsxgl_get_format_color_bit_depth(texture.pformat,3);
    }
    else if(pname == GL_RENDERBUFFER_DEPTH_SIZE) {
      *params = rsxgl_get_format_depth_bit_depth(texture.pformat,0);
    }
    else if(pname == GL_RENDERBUFFER_STENCIL_SIZE) {
      *params = rsxgl_get_format_depth_bit_depth(texture.pformat,1);
    }
    else if(pname == GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) {
      // TODO - I actually don't understand this
    }
    else if(pname == GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING) {
      // TODO
    }

    else {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
  const uint32_t rsx_framebuffer_target = rsxgl_framebuffer_target(target);
  if(rsx_framebuffer_target == RSXGL_MAX_FRAMEBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  if(!ctx -> framebuffer_binding.is_anything_bound(rsx_framebuffer_target)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_get_framebuffer_attachment_parameteriv(ctx,ctx -> framebuffer_binding.names[rsx_framebuffer_target],attachment,pname,params);
}

GLAPI void APIENTRY
glGenerateMipmap (GLenum target)
{
  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glBlitFramebuffer (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glFramebufferTextureLayer (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
  const uint32_t rsx_framebuffer_target = rsxgl_framebuffer_target(target);
  if(rsx_framebuffer_target == RSXGL_MAX_FRAMEBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  if(!ctx -> framebuffer_binding.is_anything_bound(rsx_framebuffer_target)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_framebuffer_texture(ctx,ctx -> framebuffer_binding.names[rsx_framebuffer_target],attachment,0,texture,0,level);
}

GLAPI void APIENTRY
glFramebufferTexture (GLenum target, GLenum attachment, GLuint texture, GLint level)
{
  const uint32_t rsx_framebuffer_target = rsxgl_framebuffer_target(target);
  if(rsx_framebuffer_target == RSXGL_MAX_FRAMEBUFFER_TARGETS) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();
  if(!ctx -> framebuffer_binding.is_anything_bound(rsx_framebuffer_target)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_framebuffer_texture(ctx,ctx -> framebuffer_binding.names[rsx_framebuffer_target],attachment,0,texture,0,level);
}

static inline void
rsxgl_draw_buffers(rsxgl_context_t * ctx,const framebuffer_t::name_type framebuffer_name,const GLsizei n,const GLenum buffers[RSXGL_MAX_DRAW_BUFFERS])
{ 
  framebuffer_t & framebuffer = framebuffer_t::storage().at(framebuffer_name);

  if(framebuffer.is_default) {
    if(buffers[0] == GL_BACK) {
      framebuffer.mapping.set(0,0);
    }
    else if(buffers[0] == GL_FRONT) {
      framebuffer.mapping.set(0,1);
    }
    else if(buffers[0] == GL_NONE) {
      framebuffer.mapping.set(0,RSXGL_MAX_COLOR_ATTACHMENTS);
    }
    else {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else {
    uint32_t rsx_buffers[RSXGL_MAX_DRAW_BUFFERS];

    for(size_t i = 0;i < n;++i) {
      uint32_t rsx_buffer = RSXGL_MAX_COLOR_ATTACHMENTS;

      if(buffers[i] >= GL_COLOR_ATTACHMENT0 && buffers[i] < (GL_COLOR_ATTACHMENT0 + RSXGL_MAX_COLOR_ATTACHMENTS)) {
	rsx_buffer = (uint32_t)buffers[i] - (uint32_t)GL_COLOR_ATTACHMENT0;
      }
      else if(buffers[i] == GL_NONE) {
	rsx_buffer = RSXGL_MAX_COLOR_ATTACHMENTS;
      }
      else {
	RSXGL_ERROR_(GL_INVALID_ENUM);
      }

      for(size_t j = 0;j < i;++j) {
	if(rsx_buffers[j] == rsx_buffer) RSXGL_ERROR_(GL_INVALID_OPERATION);
      }

      rsx_buffers[i] = rsx_buffer;
    }

    for(size_t i = 0;i < n;++i) {
      framebuffer.mapping.set(i,rsx_buffers[i]);
    }
  }

  framebuffer.invalid = 1;

  if(ctx -> framebuffer_binding.is_bound(RSXGL_DRAW_FRAMEBUFFER,framebuffer_name)) {
    ctx -> invalid.parts.draw_framebuffer = 1;
    ctx -> state.invalid.parts.write_mask = 1;
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDrawBuffer (GLenum mode)
{
  GLenum buffers[RSXGL_MAX_DRAW_BUFFERS];
  buffers[0] = mode;
  for(size_t i = 1;i < RSXGL_MAX_DRAW_BUFFERS;++i) {
    buffers[i] = GL_NONE;
  }
  rsxgl_draw_buffers(current_ctx(),current_ctx() -> framebuffer_binding.names[RSXGL_DRAW_FRAMEBUFFER],1,buffers);
}

GLAPI void APIENTRY
glDrawBuffers(GLsizei n, const GLenum *bufs)
{
  if(n < 0) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }
  else if(n >= RSXGL_MAX_DRAW_BUFFERS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  rsxgl_draw_buffers(current_ctx(),current_ctx() -> framebuffer_binding.names[RSXGL_DRAW_FRAMEBUFFER],n,bufs);
}

static inline void
rsxgl_emit_surface(gcmContextData * context,const uint8_t which,surface_t const & surface)
{
  static const uint32_t
    rsxgl_dma_methods[] = {
    NV30_3D_DMA_COLOR0,
    NV30_3D_DMA_COLOR1,
    NV40_3D_DMA_COLOR2,
    NV40_3D_DMA_COLOR3,
    NV30_3D_DMA_ZETA
  };
  
  static const uint32_t
    rsxgl_offset_methods[] = {
    NV30_3D_COLOR0_OFFSET,
    NV30_3D_COLOR1_OFFSET,
    NV40_3D_COLOR2_OFFSET,
    NV40_3D_COLOR3_OFFSET,
    NV30_3D_ZETA_OFFSET
  };
  
  static const uint32_t
    rsxgl_pitch_methods[] = {
    NV30_3D_COLOR0_PITCH,
    NV30_3D_COLOR1_PITCH,
    NV40_3D_COLOR2_PITCH,
    NV40_3D_COLOR3_PITCH,
    NV40_3D_ZETA_PITCH
  };

  uint32_t * buffer = gcm_reserve(context,6);
	
  gcm_emit_method_at(buffer,0,rsxgl_dma_methods[which],1);
  gcm_emit_at(buffer,1,(surface.memory.location == RSXGL_MEMORY_LOCATION_LOCAL) ? RSXGL_DMA_MEMORY_FRAME_BUFFER : RSXGL_DMA_MEMORY_HOST_BUFFER);
  
  gcm_emit_method_at(buffer,2,rsxgl_offset_methods[which],1);
  gcm_emit_at(buffer,3,surface.memory.offset);
  
  gcm_emit_method_at(buffer,4,rsxgl_pitch_methods[which],1);
  gcm_emit_at(buffer,5,surface.pitch);
  
  gcm_finish_n_commands(context,6);
}

void
rsxgl_renderbuffer_validate(rsxgl_context_t * ctx,renderbuffer_t & renderbuffer,const uint32_t timestamp)
{
}

void
rsxgl_framebuffer_validate(rsxgl_context_t * ctx,framebuffer_t & framebuffer,const uint32_t timestamp)
{
  if(!framebuffer.is_default) {
    for(framebuffer_t::attachment_types_t::const_iterator it = framebuffer.attachment_types.begin();!it.done();it.next(framebuffer.attachment_types)) {
      const uint32_t type = it.value();
      if(type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
	rsxgl_renderbuffer_validate(ctx,renderbuffer_t::storage().at(framebuffer.attachments[it.index()]),timestamp);
      }
      else if(type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
	rsxgl_texture_validate(ctx,texture_t::storage().at(framebuffer.attachments[it.index()]),timestamp);
      }
    }
  }
    
  if(framebuffer.invalid) {
    uint16_t enabled = 0;
    framebuffer_dimension_size_type w = ~0, h = ~0;
    surface_t surfaces[RSXGL_MAX_ATTACHMENTS];
    pipe_format color_pformat = PIPE_FORMAT_NONE, depth_pformat = PIPE_FORMAT_NONE;

    if(framebuffer.is_default) {
      color_pformat = ctx -> base.draw -> color_pformat;
      depth_pformat = ctx -> base.draw -> depth_pformat;

      w = ctx -> base.draw -> width;
      h = ctx -> base.draw -> height;

      if(framebuffer.attachment_types.get(0) != RSXGL_ATTACHMENT_TYPE_NONE && framebuffer.mapping.get(0) < RSXGL_MAX_COLOR_ATTACHMENTS) {
	const uint32_t buffer = (framebuffer.mapping.get(0) == 0) ? ctx -> base.draw -> buffer : !ctx -> base.draw -> buffer;
	surfaces[RSXGL_COLOR_ATTACHMENT0].pitch = ctx -> base.draw -> color_pitch;
	surfaces[RSXGL_COLOR_ATTACHMENT0].memory.location = ctx -> base.draw -> color_buffer[buffer].location;
	surfaces[RSXGL_COLOR_ATTACHMENT0].memory.offset = ctx -> base.draw -> color_buffer[buffer].offset;
	surfaces[RSXGL_COLOR_ATTACHMENT0].memory.owner = 0;
	
	enabled = NV30_3D_RT_ENABLE_COLOR0;
      }
      else {
	enabled = 0;
      }

      if(framebuffer.attachment_types.get(RSXGL_DEPTH_STENCIL_ATTACHMENT) != RSXGL_ATTACHMENT_TYPE_NONE) {
	surfaces[RSXGL_DEPTH_STENCIL_ATTACHMENT].pitch = ctx -> base.draw -> depth_pitch;
	surfaces[RSXGL_DEPTH_STENCIL_ATTACHMENT].memory.location = ctx -> base.draw -> depth_buffer.location;
	surfaces[RSXGL_DEPTH_STENCIL_ATTACHMENT].memory.offset = ctx -> base.draw -> depth_buffer.offset;
	surfaces[RSXGL_DEPTH_STENCIL_ATTACHMENT].memory.owner = 0;
      }
    }
    else {
      // Color buffers:
      for(framebuffer_t::mapping_t::const_iterator it = framebuffer.mapping.begin();!it.done();it.next(framebuffer.mapping)) {
	const framebuffer_t::attachment_size_type i = it.value();
	if(i == RSXGL_MAX_COLOR_ATTACHMENTS) continue;

	const uint32_t type = framebuffer.attachment_types.get(i);
	if(type == RSXGL_ATTACHMENT_TYPE_NONE) continue;

	if(type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
	  renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(framebuffer.attachments[i]);
	  
	  if(!util_format_is_depth_or_stencil(renderbuffer.pformat) &&
	     ctx -> screen() -> is_format_supported(ctx -> screen(),
						    renderbuffer.pformat,
						    PIPE_TEXTURE_RECT,
						    1,
						    PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET)) {
	    if(color_pformat == PIPE_FORMAT_NONE) {
	      color_pformat = renderbuffer.pformat;
	    }
	    else if(!util_is_format_compatible(util_format_description(color_pformat),
					       util_format_description(renderbuffer.pformat))) {
	      break;
	    }

	    surfaces[i] = renderbuffer.surface;
	    w = std::min(w,renderbuffer.size[0]);
	    h = std::min(h,renderbuffer.size[1]);
	    
	    enabled |= (NV30_3D_RT_ENABLE_COLOR0 << i);
	  }
	}
	else if(type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
	  texture_t & texture = texture_t::storage().at(framebuffer.attachments[i]);

	  if(texture.dims == 2 &&
	     !util_format_is_depth_or_stencil(texture.pformat) &&
	     ctx -> screen() -> is_format_supported(ctx -> screen(),
						    texture.pformat,
						    texture.rect ? PIPE_TEXTURE_RECT : PIPE_TEXTURE_2D,
						    1,
						    PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET)) {
	    if(color_pformat == PIPE_FORMAT_NONE) {
	      color_pformat = texture.pformat;
	    }
	    else if(!util_is_format_compatible(util_format_description(color_pformat),
					       util_format_description(texture.pformat))) {
	      break;
	    }
	    
	    surfaces[i].pitch = texture.pitch;
	    surfaces[i].memory = texture.memory;
	    w = std::min(w,texture.size[0]);
	    h = std::min(h,texture.size[1]);
	    
	    enabled |= (NV30_3D_RT_ENABLE_COLOR0 << i);
	  }
	}
      }

      // Depth buffer:
      {
	const uint32_t type = framebuffer.attachment_types.get(4);
	if(type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
	  renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(framebuffer.attachments[4]);
	  
	  if(util_format_is_depth_or_stencil(renderbuffer.pformat)) {
	    depth_pformat = renderbuffer.pformat;
	    
	    surfaces[4] = renderbuffer.surface;
	    w = std::min(w,renderbuffer.size[0]);
	    h = std::min(h,renderbuffer.size[1]);
	  }
	}
	else if(type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
	  texture_t & texture = texture_t::storage().at(framebuffer.attachments[4]);
	  
	  if(texture.dims == 2 &&
	     util_format_is_depth_or_stencil(texture.pformat) &&
	     ctx -> screen() -> is_format_supported(ctx -> screen(),
						    texture.pformat,
						    texture.rect ? PIPE_TEXTURE_RECT : PIPE_TEXTURE_2D,
						    1,
						    PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET)) {
	    depth_pformat = texture.pformat;
	    
	    surfaces[4].pitch = texture.pitch;
	    surfaces[4].memory = texture.memory;
	    w = std::min(w,texture.size[0]);
	    h = std::min(h,texture.size[1]);
	  }
	}
      }
    }

    rsxgl_assert(color_pformat == PIPE_FORMAT_NONE || !util_format_is_depth_or_stencil(color_pformat));
    rsxgl_assert(depth_pformat == PIPE_FORMAT_NONE || util_format_is_depth_or_stencil(depth_pformat));

    framebuffer.format = (uint16_t)nvfx_get_framebuffer_format(color_pformat,depth_pformat) | (uint16_t)NV30_3D_RT_FORMAT_TYPE_LINEAR;

    framebuffer.enabled = (enabled != 0 ? enabled : (uint16_t)NV30_3D_RT_ENABLE_COLOR0);
    framebuffer.size[0] = w;
    framebuffer.size[1] = h;

    for(framebuffer_t::attachment_size_type i = 0;i < RSXGL_MAX_ATTACHMENTS;++i) {
      framebuffer.surfaces[i] = surfaces[i];
    }

#if 0
    //
    if(color_pformat != PIPE_FORMAT_NONE && depth_pformat == PIPE_FORMAT_NONE) {
      uint32_t pitch = 0;
      for(framebuffer_t::attachment_size_type i = 0;i < RSXGL_MAX_COLOR_ATTACHMENTS && pitch == 0;++i) {
	pitch = surfaces[i].pitch;
      }
      framebuffer.surfaces[4].pitch = pitch;
    }
    else if(color_pformat == PIPE_FORMAT_NONE && depth_pformat != PIPE_FORMAT_NONE) {
      const uint32_t pitch = surfaces[4].pitch;
      for(framebuffer_t::attachment_size_type i = 0;i < RSXGL_MAX_COLOR_ATTACHMENTS;++i) {
	framebuffer.surfaces[i].pitch = pitch;
      }
    }
#endif

    framebuffer.write_mask.all = 0;

    const struct util_format_description * color_desc = util_format_description(color_pformat);
    rsxgl_assert(color_desc != 0);
    framebuffer.write_mask.parts.r = color_desc -> channel[0].type != UTIL_FORMAT_TYPE_VOID;
    framebuffer.write_mask.parts.g = color_desc -> channel[1].type != UTIL_FORMAT_TYPE_VOID;
    framebuffer.write_mask.parts.b = color_desc -> channel[2].type != UTIL_FORMAT_TYPE_VOID;
    framebuffer.write_mask.parts.a = color_desc -> channel[3].type != UTIL_FORMAT_TYPE_VOID;

    const struct util_format_description * depth_desc = util_format_description(depth_pformat);
    rsxgl_assert(depth_desc != 0);
    framebuffer.write_mask.parts.depth = depth_desc -> channel[0].type != UTIL_FORMAT_TYPE_VOID;
    framebuffer.write_mask.parts.stencil = depth_desc -> channel[1].type != UTIL_FORMAT_TYPE_VOID;

    framebuffer.invalid = 0;
  }
}

void
rsxgl_draw_framebuffer_validate(rsxgl_context_t * ctx,const uint32_t timestamp)
{
  framebuffer_t & framebuffer = ctx -> framebuffer_binding[RSXGL_DRAW_FRAMEBUFFER];

  rsxgl_framebuffer_validate(ctx,framebuffer,timestamp);

  if(ctx -> invalid.parts.draw_framebuffer) {
    gcmContextData * context = ctx -> gcm_context();

    for(framebuffer_t::attachment_size_type i = 0;i < RSXGL_MAX_ATTACHMENTS;++i) {
      rsxgl_emit_surface(context,i,framebuffer.surfaces[i]);
    }

    const uint32_t format = framebuffer.format;

    if(format != 0) {
      const uint32_t enabled = framebuffer.enabled;
      const uint16_t w = framebuffer.size[0], h = framebuffer.size[1];

      uint32_t * buffer = gcm_reserve(context,9);
      
      gcm_emit_method_at(buffer,0,NV30_3D_RT_FORMAT,1);
      gcm_emit_at(buffer,1,format | ((31 - __builtin_clz(w)) << NV30_3D_RT_FORMAT_LOG2_WIDTH__SHIFT) | ((31 - __builtin_clz(h)) << NV30_3D_RT_FORMAT_LOG2_HEIGHT__SHIFT));
      
      gcm_emit_method_at(buffer,2,NV30_3D_RT_HORIZ,2);
      gcm_emit_at(buffer,3,w << 16);
      gcm_emit_at(buffer,4,h << 16);
      
      gcm_emit_method_at(buffer,5,NV30_3D_COORD_CONVENTIONS,1);
      gcm_emit_at(buffer,6,h | NV30_3D_COORD_CONVENTIONS_ORIGIN_NORMAL);
      
      gcm_emit_method_at(buffer,7,NV30_3D_RT_ENABLE,1);
      gcm_emit_at(buffer,8,enabled);
      
      gcm_finish_n_commands(context,9);
    }
    else {
      uint32_t * buffer = gcm_reserve(context,2);

      gcm_emit_method_at(buffer,0,NV30_3D_RT_ENABLE,1);
      gcm_emit_at(buffer,1,0);
      
      gcm_finish_n_commands(context,2);
    }

    ctx -> invalid.parts.draw_framebuffer = 0;
  }
}
