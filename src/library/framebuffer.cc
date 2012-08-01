// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// framebuffer.cc - Offscreen rendering targets.

#include "rsxgl_context.h"
#include "gl_constants.h"
#include "framebuffer.h"
#include "program.h"

#include <GL3/gl3.h>
#include "error.h"

#include <rsx/gcm_sys.h>
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

  enum pipe_format
  rsxgl_choose_source_format(const GLenum format,const GLenum type);

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
						  PIPE_BIND_SAMPLER_VIEW);

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
  : is_default(0), invalid(0), invalid_complete(0), complete(0), color_pformat(PIPE_FORMAT_NONE), depth_pformat(PIPE_FORMAT_NONE),
    format(0), color_targets(0),
    color_mask(0), color_mask_mrt(0), depth_mask(0),
    read_address(0)
{
  for(size_t i = 0;i < RSXGL_MAX_ATTACHMENTS;++i) {
    attachment_types.set(i,RSXGL_ATTACHMENT_TYPE_NONE);
    attachments[i] = 0;
    attachment_layers[i] = 0;
    attachment_levels[i] = 0;
  }

  size[0] = 0;
  size[1] = 0;

  draw_buffer_mapping.set(0,0);
  for(size_t i = 1;i < RSXGL_MAX_DRAW_BUFFERS;++i) {
    draw_buffer_mapping.set(i,RSXGL_MAX_COLOR_ATTACHMENTS);
  }
  read_buffer_mapping = 0;

  for(int i = 0;i < RSXGL_MAX_COLOR_ATTACHMENTS;++i) {
    write_masks[i].parts.r = true;
    write_masks[i].parts.g = true;
    write_masks[i].parts.b = true;
    write_masks[i].parts.a = true;
  }

  write_masks[0].parts.depth = true;
  write_masks[0].parts.stencil = false;

  complete_write_mask.all = 0;
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
  framebuffer.invalid_complete = 1;

  if(ctx -> framebuffer_binding.is_bound(RSXGL_DRAW_FRAMEBUFFER,framebuffer_name)) {
    ctx -> invalid.parts.draw_framebuffer = 1;
    ctx -> state.invalid.parts.draw_framebuffer = 1;
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
    ctx -> state.invalid.parts.draw_framebuffer = 1;
  }
  if(target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) {
    ctx -> framebuffer_binding.bind(RSXGL_READ_FRAMEBUFFER,framebuffer_name);
    ctx -> invalid.parts.read_framebuffer = 1;
  }

  RSXGL_NOERROR_();
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

GLAPI void APIENTRY
glColorMask(GLboolean red,GLboolean green,GLboolean blue,GLboolean alpha)
{
  struct rsxgl_context_t * ctx = current_ctx();
  framebuffer_t & framebuffer = ctx -> framebuffer_binding[RSXGL_DRAW_FRAMEBUFFER];

  for(int i = 0;i < RSXGL_MAX_COLOR_ATTACHMENTS;++i) {
    framebuffer.write_masks[i].parts.r = red;
    framebuffer.write_masks[i].parts.g = green;
    framebuffer.write_masks[i].parts.b = blue;
    framebuffer.write_masks[i].parts.a = alpha;
  }

  framebuffer.invalid = 1;
  framebuffer.invalid_complete = 1;

  ctx -> invalid.parts.draw_framebuffer = 1;
  ctx -> state.invalid.parts.draw_framebuffer = 1;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glColorMaski(GLuint buf,GLboolean red,GLboolean green,GLboolean blue,GLboolean alpha)
{
  if(!(buf < RSXGL_MAX_COLOR_ATTACHMENTS)) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  struct rsxgl_context_t * ctx = current_ctx();
  framebuffer_t & framebuffer = ctx -> framebuffer_binding[RSXGL_DRAW_FRAMEBUFFER];

  framebuffer.write_masks[buf].parts.r = red;
  framebuffer.write_masks[buf].parts.g = green;
  framebuffer.write_masks[buf].parts.b = blue;
  framebuffer.write_masks[buf].parts.a = alpha;

  framebuffer.invalid = 1;
  framebuffer.invalid_complete = 1;

  ctx -> invalid.parts.draw_framebuffer = 1;
  ctx -> state.invalid.parts.draw_framebuffer = 1;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDepthMask (GLboolean flag)
{
  struct rsxgl_context_t * ctx = current_ctx();
  framebuffer_t & framebuffer = ctx -> framebuffer_binding[RSXGL_DRAW_FRAMEBUFFER];

  framebuffer.write_masks[0].parts.depth = flag;

  framebuffer.invalid = 1;
  framebuffer.invalid_complete = 1;

  ctx -> invalid.parts.draw_framebuffer = 1;
  ctx -> state.invalid.parts.draw_framebuffer = 1;

  RSXGL_NOERROR_();
}

static inline void
rsxgl_draw_buffers(rsxgl_context_t * ctx,const framebuffer_t::name_type framebuffer_name,const GLsizei n,const GLenum buffers[RSXGL_MAX_DRAW_BUFFERS])
{ 
  framebuffer_t & framebuffer = framebuffer_t::storage().at(framebuffer_name);

  if(framebuffer.is_default) {
    if(buffers[0] == GL_BACK) {
      framebuffer.draw_buffer_mapping.set(0,0);
    }
    else if(buffers[0] == GL_FRONT) {
      framebuffer.draw_buffer_mapping.set(0,1);
    }
    else if(buffers[0] == GL_NONE) {
      framebuffer.draw_buffer_mapping.set(0,RSXGL_MAX_COLOR_ATTACHMENTS);
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
      framebuffer.draw_buffer_mapping.set(i,rsx_buffers[i]);
    }
  }

  framebuffer.invalid = 1;
  framebuffer.invalid_complete = 1;

  if(ctx -> framebuffer_binding.is_bound(RSXGL_DRAW_FRAMEBUFFER,framebuffer_name)) {
    ctx -> invalid.parts.draw_framebuffer = 1;
    ctx -> state.invalid.parts.draw_framebuffer = 1;
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

GLAPI void APIENTRY
glReadBuffer (GLenum mode)
{
  struct rsxgl_context_t * ctx = current_ctx();
  framebuffer_t & framebuffer = ctx -> framebuffer_binding[RSXGL_READ_FRAMEBUFFER];

  uint32_t rsx_buffer = RSXGL_MAX_COLOR_ATTACHMENTS;

  if(framebuffer.is_default) {
    if(mode == GL_BACK) {
      rsx_buffer = 0;
    }
    else if(mode == GL_FRONT) {
      rsx_buffer = 1;
    }
    else if(mode == GL_NONE) {
      rsx_buffer = RSXGL_MAX_COLOR_ATTACHMENTS;
    }
    else {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }
  else {
    if(mode >= GL_COLOR_ATTACHMENT0 && mode < (GL_COLOR_ATTACHMENT0 + RSXGL_MAX_COLOR_ATTACHMENTS)) {
      rsx_buffer = (uint32_t)mode - (uint32_t)GL_COLOR_ATTACHMENT0;
    }
    else if(mode == GL_NONE) {
      rsx_buffer = RSXGL_MAX_COLOR_ATTACHMENTS;
    }
    else {
      RSXGL_ERROR_(GL_INVALID_ENUM);
    }
  }

  framebuffer.read_buffer_mapping = rsx_buffer;

  framebuffer.invalid = 1;
  framebuffer.invalid_complete = 1;

  RSXGL_NOERROR_();  
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

#if 0
  rsxgl_debug_printf("%s:%u loc:%u offset:%u pitch:%u\n",
		     __PRETTY_FUNCTION__,(unsigned int)which,
		     surface.memory.location,surface.memory.offset,surface.pitch);
#endif

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
rsxgl_renderbuffer_validate(rsxgl_context_t * ctx,renderbuffer_t & renderbuffer,uint32_t timestamp)
{
}

static inline write_mask_t
rsxgl_pformat_write_mask(const pipe_format pformat)
{
  write_mask_t result;
  result.all = 0;

  const struct util_format_description * desc = util_format_description(pformat);
  
  if(desc != 0) {
    const bool depth = util_format_has_depth(desc);
    const bool stencil = util_format_has_stencil(desc);

    if(depth || stencil) {
      result.parts.depth = desc -> channel[ desc -> swizzle[0] ].type != UTIL_FORMAT_TYPE_VOID;
      result.parts.stencil = desc -> channel[ desc -> swizzle[1] ].type != UTIL_FORMAT_TYPE_VOID;
    }
    else {
      result.parts.r = desc -> channel[ desc -> swizzle[0] ].type != UTIL_FORMAT_TYPE_VOID;
      result.parts.g = desc -> channel[ desc -> swizzle[1] ].type != UTIL_FORMAT_TYPE_VOID;
      result.parts.b = desc -> channel[ desc -> swizzle[2] ].type != UTIL_FORMAT_TYPE_VOID;
      result.parts.a = desc -> channel[ desc -> swizzle[3] ].type != UTIL_FORMAT_TYPE_VOID;
    }
  }

  return result;
}

// Check the framebuffer for completeness. Besides setting the complete predicate, also collects the
// framebuffer's pipe_formats, size, and write mask along the way:
static inline void
rsxgl_framebuffer_validate_complete(rsxgl_context_t * ctx,framebuffer_t & framebuffer)
{
  if(framebuffer.invalid_complete) {
    framebuffer_dimension_size_type w = ~0, h = ~0;
    pipe_format color_pformat = PIPE_FORMAT_NONE, depth_pformat = PIPE_FORMAT_NONE;
    write_mask_t complete_write_mask;
    complete_write_mask.all = 0;
    
    bool complete = true;

    if(framebuffer.is_default) {
      color_pformat = ctx -> base.draw -> color_pformat;
      depth_pformat = ctx -> base.draw -> depth_pformat;

      w = ctx -> base.draw -> width;
      h = ctx -> base.draw -> height;

      const write_mask_t mask = framebuffer.write_masks[0];

      complete =
	complete &&
	((mask.parts.r || mask.parts.g || mask.parts.b || mask.parts.a) ? (color_pformat != PIPE_FORMAT_NONE &&
									   !util_format_is_depth_or_stencil(color_pformat) &&
									   (ctx -> screen() -> is_format_supported(ctx -> screen(),
														   color_pformat,
														   PIPE_TEXTURE_RECT,
														   1,
														   PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET))) : true) &&
	((mask.parts.depth || mask.parts.stencil) ? (depth_pformat != PIPE_FORMAT_NONE &&
						     util_format_is_depth_or_stencil(depth_pformat) &&
						     (ctx -> screen() -> is_format_supported(ctx -> screen(),
											     depth_pformat,
											     PIPE_TEXTURE_RECT,
											     1,
											     PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_DEPTH_STENCIL))) : true);

      if(complete) {
	complete_write_mask.all |= mask.all;
      }
    }
    else {
      // Color buffers:
      for(framebuffer_t::mapping_t::const_iterator it = framebuffer.draw_buffer_mapping.begin();complete && !it.done();it.next(framebuffer.draw_buffer_mapping)) {
	const framebuffer_t::attachment_size_type i = it.value();
	if(i == RSXGL_MAX_COLOR_ATTACHMENTS) continue;

	const write_mask_t mask = framebuffer.write_masks[i];
	if(!(mask.parts.r || mask.parts.g || mask.parts.b || mask.parts.a)) continue;

	const uint32_t type = framebuffer.attachment_types.get(i);

	if(type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
	  renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(framebuffer.attachments[i]);

	  if(!util_format_is_depth_or_stencil(renderbuffer.pformat) &&
	     (ctx -> screen() -> is_format_supported(ctx -> screen(),
						     renderbuffer.pformat,
						     PIPE_TEXTURE_RECT,
						     1,
						     PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET))) {
	    if(color_pformat == PIPE_FORMAT_NONE) {
	      color_pformat = renderbuffer.pformat;
	    }
	    else {
	      complete =
		complete &&
		util_is_format_compatible(util_format_description(color_pformat),
					  util_format_description(renderbuffer.pformat));
	    }
	  }
	  else {
	    complete = false;
	  }

	  if(complete) {
	    w = std::min(w,renderbuffer.size[0]);
	    h = std::min(h,renderbuffer.size[1]);
	    complete_write_mask.all |= mask.all;
	  }
	}
	else if(type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
	  texture_t & texture = texture_t::storage().at(framebuffer.attachments[i]);
	  rsxgl_texture_validate_complete(ctx,texture);

	  if(texture.complete && texture.dims == 2 &&
	     !util_format_is_depth_or_stencil(texture.pformat) &&
	     (ctx -> screen() -> is_format_supported(ctx -> screen(),
						     texture.pformat,
						     PIPE_TEXTURE_RECT,
						     1,
						     PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET))) {
	    if(color_pformat == PIPE_FORMAT_NONE) {
	      color_pformat = texture.pformat;
	    }
	    else {
	      complete =
		complete &&
		util_is_format_compatible(util_format_description(color_pformat),
					  util_format_description(texture.pformat));
	    }
	  }
	  else {
	    complete = false;
	  }

	  if(complete) {
	    w = std::min(w,texture.size[0]);
	    h = std::min(h,texture.size[1]);
	    complete_write_mask.all |= mask.all;
	  }
	}
      }

      // Depth buffer:
      if(complete && (framebuffer.write_masks[0].parts.depth || framebuffer.write_masks[0].parts.stencil)) {
	const uint32_t type = framebuffer.attachment_types.get(4);
	if(type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
	  renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(framebuffer.attachments[4]);
	  
	  if(util_format_is_depth_or_stencil(renderbuffer.pformat) &&
	     (ctx -> screen() -> is_format_supported(ctx -> screen(),
						     renderbuffer.pformat,
						     PIPE_TEXTURE_RECT,
						     1,
						     PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_DEPTH_STENCIL))) {
	    depth_pformat = renderbuffer.pformat;
	    
	    w = std::min(w,renderbuffer.size[0]);
	    h = std::min(h,renderbuffer.size[1]);
	  }
	  else {
	    complete = false;
	  }
	}
	else if(type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
	  texture_t & texture = texture_t::storage().at(framebuffer.attachments[4]);
	  rsxgl_texture_validate_complete(ctx,texture);
	  
	  if(texture.complete && texture.dims == 2 && util_format_is_depth_or_stencil(texture.pformat) &&
	     (ctx -> screen() -> is_format_supported(ctx -> screen(),
						     texture.pformat,
						     PIPE_TEXTURE_RECT,
						     1,
						     PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_DEPTH_STENCIL))) {
	    depth_pformat = texture.pformat;
	    
	    w = std::min(w,texture.size[0]);
	    h = std::min(h,texture.size[1]);
	  }
	  else {
	    complete = false;
	  }
	}

	if(complete) {
	  complete_write_mask.parts.depth |= framebuffer.write_masks[0].parts.depth;
	  complete_write_mask.parts.stencil |= framebuffer.write_masks[0].parts.stencil;
	}
      }
    }

    if(complete) {
      framebuffer.complete = true;
      framebuffer.color_pformat = color_pformat;
      framebuffer.depth_pformat = depth_pformat;
      framebuffer.size[0] = w;
      framebuffer.size[1] = h;
      framebuffer.complete_write_mask = complete_write_mask;
    }
    else {
      framebuffer.complete = false;
      framebuffer.color_pformat = PIPE_FORMAT_NONE;
      framebuffer.depth_pformat = PIPE_FORMAT_NONE;
      framebuffer.size[0] = 0;
      framebuffer.size[1] = 0;
      framebuffer.complete_write_mask.all = 0;
    }

    framebuffer.invalid_complete = 0;
  }
}

GLAPI GLenum APIENTRY
glCheckFramebufferStatus (GLenum target)
{
  const uint32_t rsx_framebuffer_target = rsxgl_framebuffer_target(target);
  if(rsx_framebuffer_target == RSXGL_MAX_FRAMEBUFFER_TARGETS) {
    RSXGL_ERROR(GL_INVALID_ENUM,0);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(!ctx -> framebuffer_binding.is_anything_bound(rsx_framebuffer_target)) {
    RSXGL_ERROR(GL_INVALID_OPERATION,0);
  }

  framebuffer_t & framebuffer = ctx -> framebuffer_binding[rsx_framebuffer_target];
  rsxgl_framebuffer_validate_complete(ctx,framebuffer);

  if(rsx_framebuffer_target == RSXGL_DRAW_FRAMEBUFFER && framebuffer.complete) {
    RSXGL_NOERROR(GL_FRAMEBUFFER_COMPLETE);
  }
  else {
    RSXGL_NOERROR(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
  }
}

GLAPI void APIENTRY
glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
  if(width < 0 || height < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if((type == GL_UNSIGNED_BYTE_3_3_2 || type == GL_UNSIGNED_BYTE_2_3_3_REV || type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_5_6_5_REV) && (format != GL_RGB)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if((type == GL_UNSIGNED_SHORT_4_4_4_4 || type == GL_UNSIGNED_SHORT_4_4_4_4_REV || type == GL_UNSIGNED_SHORT_5_5_5_1 || type == GL_UNSIGNED_SHORT_1_5_5_5_REV || type == GL_UNSIGNED_INT_8_8_8_8 || type == GL_UNSIGNED_INT_8_8_8_8_REV || type == GL_UNSIGNED_INT_10_10_10_2 || type == GL_UNSIGNED_INT_2_10_10_10_REV) && (format != GL_RGBA || format != GL_BGRA)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  const pipe_format pdstformat = rsxgl_choose_source_format(format,type);

  if(pdstformat == PIPE_FORMAT_NONE) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_context_t * ctx = current_ctx();

  framebuffer_t & framebuffer = ctx -> framebuffer_binding[RSXGL_READ_FRAMEBUFFER];

  x = std::min(std::max(x,0),(GLint)framebuffer.size[0] - 1);
  y = std::min(std::max(y,0),(GLint)framebuffer.size[1] - 1);
  width = std::min(width,(GLsizei)framebuffer.size[0]);
  height = std::min(height,(GLsizei)framebuffer.size[1]);

  if((format == GL_STENCIL_INDEX || format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL) && (framebuffer.attachment_types.get(RSXGL_DEPTH_STENCIL_ATTACHMENT) == RSXGL_ATTACHMENT_TYPE_NONE)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(ctx -> buffer_binding.is_anything_bound(RSXGL_PIXEL_PACK_BUFFER)) {
    buffer_t & buffer = ctx -> buffer_binding[RSXGL_PIXEL_PACK_BUFFER];

    if(buffer.mapped) {
      RSXGL_ERROR_(GL_INVALID_OPERATION);
    }
  }
  else {
  }
}

void
rsxgl_framebuffer_validate(rsxgl_context_t * ctx,framebuffer_t & framebuffer,uint32_t timestamp)
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
    rsxgl_framebuffer_validate_complete(ctx,framebuffer);

    if(framebuffer.complete) {
      uint16_t color_targets = 0;
      uint32_t color_mask = 0;
      uint16_t color_mask_mrt = 0, depth_mask = 0;
      surface_t draw_surfaces[RSXGL_MAX_FRAMEBUFFER_SURFACES], read_surface;
      void * read_address = 0;

      if(framebuffer.is_default) {
	const write_mask_t mask = framebuffer.write_masks[0];

	if(framebuffer.attachment_types.get(0) != RSXGL_ATTACHMENT_TYPE_NONE && framebuffer.draw_buffer_mapping.get(0) < RSXGL_MAX_COLOR_ATTACHMENTS) {
	  const uint32_t buffer = (framebuffer.draw_buffer_mapping.get(0) == 0) ? ctx -> base.draw -> buffer : !ctx -> base.draw -> buffer;
	  draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_COLOR0].pitch = ctx -> base.draw -> color_pitch;
	  draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_COLOR0].memory.location = ctx -> base.draw -> color_buffer[buffer].location;
	  draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_COLOR0].memory.offset = ctx -> base.draw -> color_buffer[buffer].offset;
	  draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_COLOR0].memory.owner = 0;
	  
	  color_targets = NV30_3D_RT_ENABLE_COLOR0;
	  color_mask =
	    (mask.parts.r ? NV30_3D_COLOR_MASK_R : 0) |
	    (mask.parts.g ? NV30_3D_COLOR_MASK_G : 0) |
	    (mask.parts.b ? NV30_3D_COLOR_MASK_B : 0) |
	    (mask.parts.a ? NV30_3D_COLOR_MASK_A : 0);
	}
	else {
	  color_targets = 0;
	  color_mask = 0;
	}
	
	if(framebuffer.attachment_types.get(RSXGL_DEPTH_STENCIL_ATTACHMENT) != RSXGL_ATTACHMENT_TYPE_NONE) {
	  draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_DEPTH].pitch = ctx -> base.draw -> depth_pitch;
	  draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_DEPTH].memory.location = ctx -> base.draw -> depth_buffer.location;
	  draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_DEPTH].memory.offset = ctx -> base.draw -> depth_buffer.offset;
	  draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_DEPTH].memory.owner = 0;

	  depth_mask = mask.parts.depth ? 1 : 0;
	}

	if(framebuffer.read_buffer_mapping != RSXGL_MAX_COLOR_ATTACHMENTS) {
	  const uint32_t buffer = (framebuffer.read_buffer_mapping == 0) ? ctx -> base.read -> buffer : !ctx -> base.read -> buffer;
	  read_surface.pitch = ctx -> base.read -> color_pitch;
	  read_surface.memory.location = ctx -> base.read -> color_buffer[buffer].location;
	  read_surface.memory.offset = ctx -> base.read -> color_buffer[buffer].offset;
	  read_surface.memory.owner = 0;
	  read_address = ctx -> base.read -> color_address[buffer];
	}
      }
      else {
	// Color buffers:
	uint32_t i_draw_surface = RSXGL_FRAMEBUFFER_SURFACE_COLOR0;
	for(framebuffer_t::mapping_t::const_iterator it = framebuffer.draw_buffer_mapping.begin();!it.done();it.next(framebuffer.draw_buffer_mapping),++i_draw_surface) {
	  const framebuffer_t::attachment_size_type i = it.value();
	  if(i == RSXGL_MAX_COLOR_ATTACHMENTS) continue;

	  const uint32_t type = framebuffer.attachment_types.get(i);
	  if(type == RSXGL_ATTACHMENT_TYPE_NONE) continue;
	  
	  if(type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
	    renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(framebuffer.attachments[i]);
	    draw_surfaces[i_draw_surface] = renderbuffer.surface;
	  }
	  else if(type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
	    texture_t & texture = texture_t::storage().at(framebuffer.attachments[i]);
	    
	    draw_surfaces[i_draw_surface].pitch = texture.pitch;
	    draw_surfaces[i_draw_surface].memory = texture.memory;
	  }

	  const write_mask_t mask = framebuffer.write_masks[i];
	  
	  if(i == 0) {
	    color_targets |= (NV30_3D_RT_ENABLE_COLOR0 << i);
	    color_mask |=
	      (mask.parts.r ? NV30_3D_COLOR_MASK_R : 0) |
	      (mask.parts.g ? NV30_3D_COLOR_MASK_G : 0) |
	      (mask.parts.b ? NV30_3D_COLOR_MASK_B : 0) |
	      (mask.parts.a ? NV30_3D_COLOR_MASK_A : 0);
	  }
	  else {
	    color_targets |= (NV30_3D_RT_ENABLE_COLOR0 << i) | (NV30_3D_RT_ENABLE_MRT);
	    color_mask_mrt |= ((mask.parts.r ? NV40_3D_MRT_COLOR_MASK_BUFFER1_R : 0) |
			       (mask.parts.g ? NV40_3D_MRT_COLOR_MASK_BUFFER1_G : 0) |
			       (mask.parts.b ? NV40_3D_MRT_COLOR_MASK_BUFFER1_B : 0) |
			       (mask.parts.a ? NV40_3D_MRT_COLOR_MASK_BUFFER1_A : 0)) << ((i - 1) * 4);
	  }
	}

	// Depth buffer:
	if(framebuffer.attachment_types.get(RSXGL_DEPTH_STENCIL_ATTACHMENT) != RSXGL_ATTACHMENT_TYPE_NONE) {
	  const uint32_t type = framebuffer.attachment_types.get(RSXGL_DEPTH_STENCIL_ATTACHMENT);
	  if(type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
	    renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(framebuffer.attachments[RSXGL_DEPTH_STENCIL_ATTACHMENT]);
	    draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_DEPTH] = renderbuffer.surface;
	  }
	  else if(type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
	    texture_t & texture = texture_t::storage().at(framebuffer.attachments[RSXGL_DEPTH_STENCIL_ATTACHMENT]);
	    draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_DEPTH].pitch = texture.pitch;
	    draw_surfaces[RSXGL_FRAMEBUFFER_SURFACE_DEPTH].memory = texture.memory;
	  }

	  const write_mask_t mask = framebuffer.write_masks[0];
	  depth_mask = mask.parts.depth ? 1 : 0;
	}

	// Read buffer:
	if(framebuffer.read_buffer_mapping != RSXGL_MAX_COLOR_ATTACHMENTS) {
	  const uint32_t read_buffer_attachment = framebuffer.read_buffer_mapping;
	  const uint32_t type = framebuffer.attachment_types.get(read_buffer_attachment);
	  if(type == RSXGL_ATTACHMENT_TYPE_RENDERBUFFER) {
	    renderbuffer_t & renderbuffer = renderbuffer_t::storage().at(framebuffer.attachments[read_buffer_attachment]);
	    read_surface = renderbuffer.surface;
	    read_address = rsxgl_arena_address(memory_arena_t::storage().at(renderbuffer.arena),renderbuffer.surface.memory);
	  }
	  else if(type == RSXGL_ATTACHMENT_TYPE_TEXTURE) {
	    texture_t & texture = texture_t::storage().at(framebuffer.attachments[read_buffer_attachment]);
	    read_surface.pitch = texture.pitch;
	    read_surface.memory = texture.memory;
	    read_address = rsxgl_arena_address(memory_arena_t::storage().at(texture.arena),texture.memory);
	  }
	}
      }

      // Store data that gets passed down the command stream:
      framebuffer.format = (uint16_t)nvfx_get_framebuffer_format(framebuffer.color_pformat,framebuffer.depth_pformat) | (uint16_t)NV30_3D_RT_FORMAT_TYPE_LINEAR;
      framebuffer.color_targets = color_targets;
      framebuffer.color_mask = color_mask;
      framebuffer.color_mask_mrt = color_mask_mrt;
      framebuffer.depth_mask = depth_mask;
      
      for(framebuffer_t::attachment_size_type i = 0;i < RSXGL_MAX_FRAMEBUFFER_SURFACES;++i) {
	framebuffer.draw_surfaces[i] = draw_surfaces[i];
      }
      framebuffer.read_surface = read_surface;
      framebuffer.read_address = read_address;
      
#if 0
      //
      if(color_pformat != PIPE_FORMAT_NONE && depth_pformat == PIPE_FORMAT_NONE) {
	uint32_t pitch = 0;
	for(framebuffer_t::attachment_size_type i = 0;i < RSXGL_MAX_COLOR_ATTACHMENTS && pitch == 0;++i) {
	  pitch = draw_surfaces[i].pitch;
	}
	framebuffer.draw_surfaces[4].pitch = pitch;
      }
      else if(color_pformat == PIPE_FORMAT_NONE && depth_pformat != PIPE_FORMAT_NONE) {
	const uint32_t pitch = draw_surfaces[4].pitch;
	for(framebuffer_t::attachment_size_type i = 0;i < RSXGL_MAX_COLOR_ATTACHMENTS;++i) {
	  framebuffer.draw_surfaces[i].pitch = pitch;
	}
      }
#endif
    }
    
    framebuffer.invalid = 0;
  }
}

void
rsxgl_draw_framebuffer_validate(rsxgl_context_t * ctx,uint32_t timestamp)
{
  framebuffer_t & framebuffer = ctx -> framebuffer_binding[RSXGL_DRAW_FRAMEBUFFER];

  rsxgl_framebuffer_validate(ctx,framebuffer,timestamp);

  if(ctx -> invalid.parts.draw_framebuffer) {
    if(framebuffer.complete) {
      const uint32_t format = framebuffer.format;
      const uint32_t color_targets = framebuffer.color_targets;
      const uint32_t color_mask = framebuffer.color_mask;
      const uint32_t color_mask_mrt = framebuffer.color_mask_mrt;
      const uint32_t depth_mask = framebuffer.depth_mask;

      gcmContextData * context = ctx -> gcm_context();
      
      if(format != 0 && color_targets != 0) {
	for(framebuffer_t::attachment_size_type i = 0;i < RSXGL_MAX_FRAMEBUFFER_SURFACES;++i) {
	  rsxgl_emit_surface(context,i,framebuffer.draw_surfaces[i]);
	}

	const uint16_t w = framebuffer.size[0], h = framebuffer.size[1];

#if 0
	rsxgl_debug_printf("%s format:%x color_targets:%x size:%ux%u color_mask:%x color_mask_mrt:%x depth_mask:%x\n",__PRETTY_FUNCTION__,
			   format,color_targets,(unsigned int)w,(unsigned int)h,color_mask,color_mask_mrt,depth_mask);
#endif
	
	uint32_t * buffer = gcm_reserve(context,15);
	
	gcm_emit_method_at(buffer,0,NV30_3D_RT_FORMAT,1);
	gcm_emit_at(buffer,1,format | ((31 - __builtin_clz(w)) << NV30_3D_RT_FORMAT_LOG2_WIDTH__SHIFT) | ((31 - __builtin_clz(h)) << NV30_3D_RT_FORMAT_LOG2_HEIGHT__SHIFT));
	
	gcm_emit_method_at(buffer,2,NV30_3D_RT_HORIZ,2);
	gcm_emit_at(buffer,3,w << 16);
	gcm_emit_at(buffer,4,h << 16);
	
	gcm_emit_method_at(buffer,5,NV30_3D_COORD_CONVENTIONS,1);
	gcm_emit_at(buffer,6,h | NV30_3D_COORD_CONVENTIONS_ORIGIN_NORMAL);
	
	gcm_emit_method_at(buffer,7,NV30_3D_RT_ENABLE,1);
	gcm_emit_at(buffer,8,color_targets);

	gcm_emit_method_at(buffer,9,NV30_3D_COLOR_MASK,1);
	gcm_emit_at(buffer,10,color_mask);

	gcm_emit_method_at(buffer,11,NV40_3D_MRT_COLOR_MASK,1);
	gcm_emit_at(buffer,12,color_mask_mrt);

	gcm_emit_method_at(buffer,13,NV30_3D_DEPTH_WRITE_ENABLE,1);
	gcm_emit_at(buffer,14,depth_mask);
	
	gcm_finish_n_commands(context,15);
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
}

bool
rsxgl_feedback_framebuffer_check(rsxgl_context_t * ctx,uint32_t offset,uint32_t count)
{
  const program_t & program = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM];
  rsxgl_assert(program.streamfp_num_outputs <= RSXGL_MAX_COLOR_ATTACHMENTS);

  const uint32_t attrib_stride = sizeof(float) * 4;
  const uint32_t length = attrib_stride * count;

  size_t binding = RSXGL_TRANSFORM_FEEDBACK_BUFFER0, range_binding = RSXGL_TRANSFORM_FEEDBACK_BUFFER_RANGE0;
  for(unsigned int i = 0;i < program.streamfp_num_outputs;++i,++binding,++range_binding) {
    if(ctx -> buffer_binding.names[binding] == 0 ||
       (offset + length) > ctx -> buffer_binding_offset_size[range_binding].second) {
      return false;
    }
  }

  return true;
}

void
rsxgl_feedback_framebuffer_validate(rsxgl_context_t * ctx,uint32_t offset,uint32_t count,uint16_t * pw,uint16_t * ph,uint32_t timestamp)
{
  gcmContextData * context = ctx -> gcm_context();

  const program_t & program = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM];

  const uint32_t format = (NV30_3D_RT_FORMAT_COLOR_A32B32G32R32_FLOAT | NV30_3D_RT_FORMAT_ZETA_Z24S8 | NV30_3D_RT_FORMAT_TYPE_LINEAR);
  //const uint32_t format = 0x128;
  uint16_t color_targets = 0;
  uint32_t color_mask = 0;
  uint16_t color_mask_mrt = 0;
  const uint16_t depth_mask = 0;

  const uint32_t attrib_length = sizeof(float) * 4;
  const uint32_t length = attrib_length * count;
  const uint32_t pitch = attrib_length * RSXGL_MAX_RENDERBUFFER_SIZE;

  size_t binding = RSXGL_TRANSFORM_FEEDBACK_BUFFER0, range_binding = RSXGL_TRANSFORM_FEEDBACK_BUFFER_RANGE0, surface = RSXGL_FRAMEBUFFER_SURFACE_COLOR0;
  for(unsigned int i = 0;i < program.streamfp_num_outputs;++i,++binding,++range_binding,++surface) {
    rsxgl_assert(ctx -> buffer_binding.names[binding] != 0 && (offset + length) < ctx -> buffer_binding_offset_size[range_binding].second);

    buffer_t & buffer = ctx -> buffer_binding[binding];
    const uint32_t buffer_offset = ctx -> buffer_binding_offset_size[range_binding].first + offset;

    rsxgl_buffer_validate(ctx,buffer,buffer_offset,length,timestamp);

    rsxgl_emit_surface(context,surface,surface_t(buffer.memory + buffer_offset,pitch));

    if(i == 0) {
      color_targets |= (NV30_3D_RT_ENABLE_COLOR0);
      color_mask |= (NV30_3D_COLOR_MASK_R | NV30_3D_COLOR_MASK_G | NV30_3D_COLOR_MASK_B | NV30_3D_COLOR_MASK_A);
    }
    else {
      color_targets |= (NV30_3D_RT_ENABLE_COLOR0 << i) | (NV30_3D_RT_ENABLE_MRT);
      color_mask_mrt |= (NV40_3D_MRT_COLOR_MASK_BUFFER1_R |
			 NV40_3D_MRT_COLOR_MASK_BUFFER1_G |
			 NV40_3D_MRT_COLOR_MASK_BUFFER1_B |
			 NV40_3D_MRT_COLOR_MASK_BUFFER1_A) << ((i - 1) * 4);
    }
  }

  rsxgl_emit_surface(context,RSXGL_FRAMEBUFFER_SURFACE_DEPTH,surface_t());

  const uint16_t div = count / RSXGL_MAX_RENDERBUFFER_SIZE;
  const uint16_t mod = count % RSXGL_MAX_RENDERBUFFER_SIZE;

  const uint16_t w = (div > 0) ? RSXGL_MAX_RENDERBUFFER_SIZE : mod;
  //const uint16_t h = div + ((mod > 0) ? 1 : 0);
  const uint16_t h = w;

#if 0
  rsxgl_debug_printf("%s format:%x color_targets:%x size:%ux%u color_mask:%x color_mask_mrt:%x depth_mask:%x %ux%u\n",__PRETTY_FUNCTION__,
		     format,color_targets,(unsigned int)w,(unsigned int)h,color_mask,color_mask_mrt,depth_mask,(unsigned int)w,(unsigned int)h);
#endif

  uint32_t * buffer = gcm_reserve(context,15);
  
  gcm_emit_method_at(buffer,0,NV30_3D_RT_FORMAT,1);
  gcm_emit_at(buffer,1,format | ((31 - __builtin_clz(w)) << NV30_3D_RT_FORMAT_LOG2_WIDTH__SHIFT) | ((31 - __builtin_clz(h)) << NV30_3D_RT_FORMAT_LOG2_HEIGHT__SHIFT));
  
  gcm_emit_method_at(buffer,2,NV30_3D_RT_HORIZ,2);
  gcm_emit_at(buffer,3,w << 16);
  gcm_emit_at(buffer,4,h << 16);
  
  gcm_emit_method_at(buffer,5,NV30_3D_COORD_CONVENTIONS,1);
  gcm_emit_at(buffer,6,h | NV30_3D_COORD_CONVENTIONS_ORIGIN_NORMAL | NV30_3D_COORD_CONVENTIONS_CENTER_INTEGER);
  
  gcm_emit_method_at(buffer,7,NV30_3D_RT_ENABLE,1);
  gcm_emit_at(buffer,8,color_targets);
  
  gcm_emit_method_at(buffer,9,NV30_3D_COLOR_MASK,1);
  gcm_emit_at(buffer,10,color_mask);
  
  gcm_emit_method_at(buffer,11,NV40_3D_MRT_COLOR_MASK,1);
  gcm_emit_at(buffer,12,color_mask_mrt);
  
  gcm_emit_method_at(buffer,13,NV30_3D_DEPTH_WRITE_ENABLE,1);
  gcm_emit_at(buffer,14,depth_mask);
  
  gcm_finish_n_commands(context,15);

  *pw = w;
  *ph = h;
}
