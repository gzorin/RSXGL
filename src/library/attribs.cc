// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// attribs.cc - Set program vertex attribute data.

#include "gl_constants.h"
#include "rsxgl_context.h"
#include "debug.h"
#include "arena.h"
#include "buffer.h"
#include "attribs.h"

#include <GL3/gl3.h>
#include "error.h"

#include <rsx/gcm_sys.h>
#include "nv40.h"
#include "gl_fifo.h"

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

attribs_t::storage_type & attribs_t::storage()
{
  return current_object_ctx() -> attribs_storage();
}

attribs_t::~attribs_t()
{
}

GLAPI void APIENTRY
glBindVertexArray (GLuint attribs_name)
{
  if(!(attribs_name == 0 || attribs_t::storage().is_name(attribs_name))) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  struct rsxgl_context_t * ctx = current_ctx();

  if(attribs_name != 0 && !attribs_t::storage().is_object(attribs_name)) {
    attribs_t::storage().create_object(attribs_name);
  }

  ctx -> attribs_binding.bind(0,attribs_name);
  ctx -> invalid_attribs.set();
  
  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDeleteVertexArrays (GLsizei n, const GLuint *arrays)
{
  struct rsxgl_context_t * ctx = current_ctx();

  for(GLsizei i = 0;i < n;++i,++arrays) {
    const GLuint attribs_name = *arrays;

    if(attribs_name == 0) continue;

    // Free resources used by this object:
    if(attribs_t::storage().is_object(attribs_name)) {
      // If this attribs_name is bound to any attribs_name targets, unbind it from them:
      ctx -> attribs_binding.unbind_from_all(attribs_name);
      attribs_t::storage().destroy(attribs_name);
    }
    else if(attribs_t::storage().is_name(attribs_name)) {
      attribs_t::storage().destroy(attribs_name);
    }
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGenVertexArrays (GLsizei n, GLuint *attribs)
{
  GLsizei count = attribs_t::storage().create_names(n,attribs);

  if(count != n) {
    RSXGL_ERROR_(GL_OUT_OF_MEMORY);
  }

  RSXGL_NOERROR_();
}

GLAPI GLboolean APIENTRY
glIsVertexArray (GLuint attribs)
{
  return attribs_t::storage().is_object(attribs);
}

GLAPI void APIENTRY
glEnableVertexAttribArray (GLuint index)
{
  if(index >= RSXGL_MAX_VERTEX_ATTRIBS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  struct rsxgl_context_t * ctx = current_ctx();
  attribs_t & attribs = ctx -> attribs_binding[0];

  attribs.enabled.set(index);

  ctx -> invalid_attribs.set(index);

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDisableVertexAttribArray (GLuint index)
{
  if(index >= RSXGL_MAX_VERTEX_ATTRIBS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  struct rsxgl_context_t * ctx = current_ctx();
  attribs_t & attribs = ctx -> attribs_binding[0];

  attribs.enabled.reset(index);

  ctx -> invalid_attribs.set(index);

  RSXGL_NOERROR_();
}

static inline void
rsxgl_get_vertex_attribi(rsxgl_context_t * ctx,GLuint index,GLenum pname,uint32_t * param)
{
  if(index >= RSXGL_MAX_VERTEX_ATTRIBS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  attribs_t & attribs = ctx -> attribs_binding[0];

  if(pname == GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING) {
    *param = attribs.buffers.names[index];
  }
  else if(pname == GL_VERTEX_ATTRIB_ARRAY_ENABLED) {
    *param = attribs.enabled.get(index);
  }
  else if(pname == GL_VERTEX_ATTRIB_ARRAY_SIZE) {
    *param = attribs.size.get(index);
  }
  else if(pname == GL_VERTEX_ATTRIB_ARRAY_STRIDE) {
    *param = attribs.stride[index];
  }
  else if(pname == GL_VERTEX_ATTRIB_ARRAY_TYPE) {
    uint32_t rsx_type = attribs.type.get(index);

    if(rsx_type == RSXGL_VERTEX_U8_NR || rsx_type == RSXGL_VERTEX_U8_UN) {
      *param = GL_UNSIGNED_BYTE;
    }
    else if(rsx_type == RSXGL_VERTEX_S16_NR || rsx_type == RSXGL_VERTEX_S16_UN) {
      *param = GL_UNSIGNED_SHORT;
    }
    else if(rsx_type == RSXGL_VERTEX_F32) {
      *param = GL_FLOAT;
    }
    else if(rsx_type == RSXGL_VERTEX_F16) {
      *param = GL_HALF_FLOAT;
    }
    else if(rsx_type == RSXGL_VERTEX_S11_11_10_NR) {
      *param = GL_UNSIGNED_INT_2_10_10_10_REV;
    }
  }
  else if(pname == GL_VERTEX_ATTRIB_ARRAY_NORMALIZED) {
    uint32_t rsx_type = attribs.type.get(index);

    if(rsx_type == RSXGL_VERTEX_U8_NR || rsx_type == RSXGL_VERTEX_S16_NR || rsx_type == RSXGL_VERTEX_S11_11_10_NR) {
      *param = GL_TRUE;
    }
    else {
      *param = GL_FALSE;
    }
  }
  else if(pname == GL_VERTEX_ATTRIB_ARRAY_INTEGER) {
    uint32_t rsx_type = attribs.type.get(index);

    if(rsx_type == RSXGL_VERTEX_U8_NR ||
       rsx_type == RSXGL_VERTEX_U8_UN ||
       rsx_type == RSXGL_VERTEX_S16_NR ||
       rsx_type == RSXGL_VERTEX_S16_UN ||
       rsx_type == RSXGL_VERTEX_S11_11_10_NR) {
      *param = GL_TRUE;
    }
    else {
      *param = GL_FALSE;
    }
  }
  else {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glGetVertexAttribdv (GLuint index, GLenum pname, GLdouble *params)
{
  if(pname == GL_CURRENT_VERTEX_ATTRIB) {
  }
  else {
    uint32_t _param = 0;
    rsxgl_get_vertex_attribi(current_ctx(),index,pname,(uint32_t *)params);
    *params = _param;
  }
}

GLAPI void APIENTRY
glGetVertexAttribfv (GLuint index, GLenum pname, GLfloat* params)
{
  if(pname == GL_CURRENT_VERTEX_ATTRIB) {
  }
  else {
    uint32_t _param = 0;
    rsxgl_get_vertex_attribi(current_ctx(),index,pname,(uint32_t *)params);
    *params = _param;
  }
}

GLAPI void APIENTRY
glGetVertexAttribiv (GLuint index, GLenum pname, GLint* params)
{
  rsxgl_get_vertex_attribi(current_ctx(),index,pname,(uint32_t *)params);
}

GLAPI void APIENTRY
glGetVertexAttribPointerv (GLuint index, GLenum pname, GLvoid** pointer)
{
  if(index >= RSXGL_MAX_VERTEX_ATTRIBS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(!(pname == GL_VERTEX_ATTRIB_ARRAY_POINTER)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  struct rsxgl_context_t * ctx = current_ctx();
  
  if(ctx -> attribs_binding.names[0] == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  attribs_t & attribs = ctx -> attribs_binding[0];

  if(pname == GL_VERTEX_ATTRIB_ARRAY_POINTER) {
    *pointer = (GLvoid *)((uint64_t)attribs.offset[index]);
  }

  RSXGL_NOERROR_();
}

template< typename Type, size_t Size >
static inline void
rsxgl_vertex_attrib(rsxgl_context_t * ctx,uint32_t rsx_type,GLuint index,const Type & v0 = 0,const Type & v1 = 0,const Type & v2 = 0,const Type & v3 = 0)
{
  if(index >= RSXGL_MAX_VERTEX_ATTRIBS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  attribs_t & attribs = ctx -> attribs_binding[0];

  attribs.buffers.bind(index,0);
  attribs.offset[index] = 0;

  set_gpu_data(attribs.defaults[index][0],v0);
  if(Size >= 1) set_gpu_data(attribs.defaults[index][1],v1);
  if(Size >= 2) set_gpu_data(attribs.defaults[index][2],v2);
  if(Size >= 3) set_gpu_data(attribs.defaults[index][3],v3);

  ctx -> invalid_attribs.set(index);
  
  RSXGL_NOERROR_();
}

//
GLAPI void APIENTRY
glVertexAttrib1d (GLuint index, GLdouble x)
{
  rsxgl_vertex_attrib< GLfloat, 1 >(current_ctx(),RSXGL_VERTEX_F32,index,(GLfloat)x);
}

GLAPI void APIENTRY
glVertexAttrib1dv (GLuint index, const GLdouble *v)
{
  rsxgl_vertex_attrib< GLfloat, 1 >(current_ctx(),RSXGL_VERTEX_F32,index,(GLfloat)v[0]);
}

GLAPI void APIENTRY
glVertexAttrib1f (GLuint index, GLfloat x)
{
  rsxgl_vertex_attrib< GLfloat, 1 >(current_ctx(),RSXGL_VERTEX_F32,index,x);
}

GLAPI void APIENTRY
glVertexAttrib1fv (GLuint index, const GLfloat *v)
{
  rsxgl_vertex_attrib< GLfloat, 1 >(current_ctx(),RSXGL_VERTEX_F32,index,v[0]);
}

GLAPI void APIENTRY
glVertexAttrib1s (GLuint index, GLshort x)
{
  rsxgl_vertex_attrib< GLshort, 1 >(current_ctx(),RSXGL_VERTEX_S16_NR,index,x);
}

GLAPI void APIENTRY
glVertexAttrib1sv (GLuint index, const GLshort *v)
{
  rsxgl_vertex_attrib< GLshort, 1 >(current_ctx(),RSXGL_VERTEX_S16_NR,index,v[0]);
}

//
GLAPI void APIENTRY
glVertexAttrib2d (GLuint index, GLdouble x, GLdouble y)
{
  rsxgl_vertex_attrib< GLfloat, 2 >(current_ctx(),RSXGL_VERTEX_F32,index,(float)x,(float)y);
}

GLAPI void APIENTRY
glVertexAttrib2dv (GLuint index, const GLdouble *v)
{
  rsxgl_vertex_attrib< GLfloat, 2 >(current_ctx(),RSXGL_VERTEX_F32,index,(float)v[0],(float)v[1]);
}

GLAPI void APIENTRY
glVertexAttrib2f (GLuint index, GLfloat x, GLfloat y)
{
  rsxgl_vertex_attrib< GLfloat, 2 >(current_ctx(),RSXGL_VERTEX_F32,index,x,y);
}

GLAPI void APIENTRY
glVertexAttrib2fv (GLuint index, const GLfloat *v)
{
  rsxgl_vertex_attrib< GLfloat, 2 >(current_ctx(),RSXGL_VERTEX_F32,index,v[0],v[1]);
}

GLAPI void APIENTRY
glVertexAttrib2s (GLuint index, GLshort x, GLshort y)
{
  rsxgl_vertex_attrib< GLshort, 2 >(current_ctx(),RSXGL_VERTEX_S16_NR,index,x,y);
}

GLAPI void APIENTRY
glVertexAttrib2sv (GLuint index, const GLshort *v)
{
  rsxgl_vertex_attrib< GLshort, 2 >(current_ctx(),RSXGL_VERTEX_S16_NR,index,v[0],v[1]);
}

//
GLAPI void APIENTRY
glVertexAttrib3d (GLuint index, GLdouble x, GLdouble y, GLdouble z)
{
  rsxgl_vertex_attrib< GLfloat, 3 >(current_ctx(),RSXGL_VERTEX_F32,index,(float)x,(float)y,(float)z);
}

GLAPI void APIENTRY
glVertexAttrib3dv (GLuint index, const GLdouble *v)
{
  rsxgl_vertex_attrib< GLfloat, 3 >(current_ctx(),RSXGL_VERTEX_F32,index,(float)v[0],(float)v[1],(float)v[2]);
}

GLAPI void APIENTRY
glVertexAttrib3f (GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
  rsxgl_vertex_attrib< GLfloat, 3 >(current_ctx(),RSXGL_VERTEX_F32,index,x,y,z);
}

GLAPI void APIENTRY
glVertexAttrib3fv (GLuint index, const GLfloat *v)
{
  rsxgl_vertex_attrib< GLfloat, 3 >(current_ctx(),RSXGL_VERTEX_F32,index,v[0],v[1],v[2]);
}

GLAPI void APIENTRY
glVertexAttrib3s (GLuint index, GLshort x, GLshort y, GLshort z)
{
  rsxgl_vertex_attrib< GLshort, 3 >(current_ctx(),RSXGL_VERTEX_S16_NR,index,x,y,z);
}

GLAPI void APIENTRY
glVertexAttrib3sv (GLuint index, const GLshort *v)
{
  rsxgl_vertex_attrib< GLshort, 3 >(current_ctx(),RSXGL_VERTEX_S16_NR,index,v[0],v[1],v[2]);
}

//
GLAPI void APIENTRY
glVertexAttrib4Nbv (GLuint index, const GLbyte *v)
{
  rsxgl_vertex_attrib< GLbyte, 4 >(current_ctx(),RSXGL_VERTEX_S16_NR,index,v[0],v[1],v[2]);
}

GLAPI void APIENTRY
glVertexAttrib4Niv (GLuint index, const GLint *v)
{
  rsxgl_vertex_attrib< GLint, 4 >(current_ctx(),RSXGL_VERTEX_F32,index,v[0],v[1],v[2]);
}

GLAPI void APIENTRY
glVertexAttrib4Nsv (GLuint index, const GLshort *v)
{
  rsxgl_vertex_attrib< GLshort, 4 >(current_ctx(),RSXGL_VERTEX_F32,index,v[0],v[1],v[2]);
}

GLAPI void APIENTRY
glVertexAttrib4Nub (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w)
{
  rsxgl_vertex_attrib< GLubyte, 4 >(current_ctx(),RSXGL_VERTEX_F32,index,x,y,z,w);
}

GLAPI void APIENTRY
glVertexAttrib4Nubv (GLuint index, const GLubyte *v)
{
  rsxgl_vertex_attrib< GLubyte, 4 >(current_ctx(),RSXGL_VERTEX_F32,index,v[0],v[1],v[2]);
}

GLAPI void APIENTRY
glVertexAttrib4Nuiv (GLuint index, const GLuint *v)
{
}

GLAPI void APIENTRY
glVertexAttrib4Nusv (GLuint index, const GLushort *v)
{
}

GLAPI void APIENTRY
glVertexAttrib4bv (GLuint index, const GLbyte *v)
{
}

GLAPI void APIENTRY
glVertexAttrib4d (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
  rsxgl_vertex_attrib< GLfloat, 3 >(current_ctx(),RSXGL_VERTEX_F32,index,(float)x,(float)y,(float)z,(float)w);
}

GLAPI void APIENTRY
glVertexAttrib4dv (GLuint index, const GLdouble *v)
{
  rsxgl_vertex_attrib< GLfloat, 3 >(current_ctx(),RSXGL_VERTEX_F32,index,(float)v[0],(float)v[1],(float)v[2],(float)v[3]);
}

GLAPI void APIENTRY
glVertexAttrib4f (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
  rsxgl_vertex_attrib< GLfloat, 3 >(current_ctx(),RSXGL_VERTEX_F32,index,(float)x,(float)y,(float)z,(float)w);
}

GLAPI void APIENTRY
glVertexAttrib4fv (GLuint index, const GLfloat *v)
{
  rsxgl_vertex_attrib< GLfloat, 3 >(current_ctx(),RSXGL_VERTEX_F32,index,(float)v[0],(float)v[1],(float)v[2],(float)v[3]);
}

GLAPI void APIENTRY
glVertexAttrib4iv (GLuint index, const GLint *v)
{
}

GLAPI void APIENTRY
glVertexAttrib4s (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w)
{
}

GLAPI void APIENTRY
glVertexAttrib4sv (GLuint index, const GLshort *v)
{
}

GLAPI void APIENTRY
glVertexAttrib4ubv (GLuint index, const GLubyte *v)
{
}

GLAPI void APIENTRY
glVertexAttrib4uiv (GLuint index, const GLuint *v)
{
}

GLAPI void APIENTRY
glVertexAttrib4usv (GLuint index, const GLushort *v)
{
}

GLAPI void APIENTRY
glVertexAttribI1i (GLuint index, GLint x)
{
}

GLAPI void APIENTRY
glVertexAttribI2i (GLuint index, GLint x, GLint y)
{
}

GLAPI void APIENTRY
glVertexAttribI3i (GLuint index, GLint x, GLint y, GLint z)
{
}

GLAPI void APIENTRY
glVertexAttribI4i (GLuint index, GLint x, GLint y, GLint z, GLint w)
{
}

GLAPI void APIENTRY
glVertexAttribI1ui (GLuint index, GLuint x)
{
}

GLAPI void APIENTRY
glVertexAttribI2ui (GLuint index, GLuint x, GLuint y)
{
}

GLAPI void APIENTRY
glVertexAttribI3ui (GLuint index, GLuint x, GLuint y, GLuint z)
{
}

GLAPI void APIENTRY
glVertexAttribI4ui (GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
}

GLAPI void APIENTRY
glVertexAttribI1iv (GLuint index, const GLint *v)
{
}

GLAPI void APIENTRY
glVertexAttribI2iv (GLuint index, const GLint *v)
{
}

GLAPI void APIENTRY
glVertexAttribI3iv (GLuint index, const GLint *v)
{
}

GLAPI void APIENTRY
glVertexAttribI4iv (GLuint index, const GLint *v)
{
}

GLAPI void APIENTRY
glVertexAttribI1uiv (GLuint index, const GLuint *v)
{
}

GLAPI void APIENTRY
glVertexAttribI2uiv (GLuint index, const GLuint *v)
{
}

GLAPI void APIENTRY
glVertexAttribI3uiv (GLuint index, const GLuint *v)
{
}

GLAPI void APIENTRY
glVertexAttribI4uiv (GLuint index, const GLuint *v)
{
}

GLAPI void APIENTRY
glVertexAttribI4bv (GLuint index, const GLbyte *v)
{
}

GLAPI void APIENTRY
glVertexAttribI4sv (GLuint index, const GLshort *v)
{
}

GLAPI void APIENTRY
glVertexAttribI4ubv (GLuint index, const GLubyte *v)
{
}

GLAPI void APIENTRY
glVertexAttribI4usv (GLuint index, const GLushort *v)
{
}

GLAPI void APIENTRY
glVertexAttribP1ui (GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
}

GLAPI void APIENTRY
glVertexAttribP1uiv (GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
}

GLAPI void APIENTRY
glVertexAttribP2ui (GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
}

GLAPI void APIENTRY
glVertexAttribP2uiv (GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
}

GLAPI void APIENTRY
glVertexAttribP3ui (GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
}

GLAPI void APIENTRY
glVertexAttribP3uiv (GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
}

GLAPI void APIENTRY
glVertexAttribP4ui (GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
}

GLAPI void APIENTRY
glVertexAttribP4uiv (GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
}

static inline uint32_t
rsxgl_vertex_buffer_type(GLenum type,GLboolean normalized)
{
  switch(type) {
  case GL_BYTE:
  case GL_UNSIGNED_BYTE:
    if(normalized) return RSXGL_VERTEX_U8_NR;
    else return RSXGL_VERTEX_U8_UN;
  case GL_SHORT:
  case GL_UNSIGNED_SHORT:
    if(normalized) return RSXGL_VERTEX_S16_NR;
    else return RSXGL_VERTEX_S16_UN;
  case GL_FLOAT:
    return RSXGL_VERTEX_F32;
  case GL_HALF_FLOAT:
    return RSXGL_VERTEX_F16;
  case GL_INT_2_10_10_10_REV:
  case GL_UNSIGNED_INT_2_10_10_10_REV:
    return RSXGL_VERTEX_S11_11_10_NR;
  default:
    return 0;
  }
}

static inline uint32_t
rsxgl_vertex_buffer_stride(GLenum type)
{
  switch(type) {
  case GL_BYTE:
  case GL_UNSIGNED_BYTE:
    return sizeof(uint8_t);
  case GL_SHORT:
  case GL_UNSIGNED_SHORT:
    return sizeof(uint16_t);
  case GL_FLOAT:
    return sizeof(float);
  case GL_HALF_FLOAT:
    return sizeof(float) / 2;
  case GL_INT_2_10_10_10_REV:
  case GL_UNSIGNED_INT_2_10_10_10_REV:
    return 4;
  default:
    return 0;
  }
}

static inline void
rsxgl_vertex_attrib_pointer(rsxgl_context_t * ctx,uint32_t rsx_type,GLuint index, GLint size, uint32_t stride, const GLvoid *pointer)
{
  if(index >= RSXGL_MAX_VERTEX_ATTRIBS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(size < 1 || size > 4) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(stride < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

#if (RSXGL_CONFIG_client_attribs == 0)
  if(ctx -> buffer_binding.names[RSXGL_ARRAY_BUFFER] == 0 && pointer != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }
#endif

  attribs_t & attribs = ctx -> attribs_binding[0];

#if (RSXGL_CONFIG_client_attribs == 1)
  if(ctx -> buffer_binding.names[RSXGL_ARRAY_BUFFER] != 0 || pointer != 0) {
    if(ctx -> buffer_binding.names[RSXGL_ARRAY_BUFFER] != 0) {
      attribs.buffers.bind(index,ctx -> buffer_binding.names[RSXGL_ARRAY_BUFFER]);
      attribs.offset[index] = rsxgl_pointer_to_offset(pointer);
      attribs.client_buffer[i] = 0;
      attribs.client_buffer_enabled.reset(index);
    }
    else {
      attribs.buffers.bind(index,0);
      attribs.offset[index] = 0;
      attribs.client_buffer[i] = pointer;
      attribs.client_buffer_enabled.set(index);
    }
    attribs.type.set(index,rsx_type);
    attribs.size.set(index,size - 1);
    attribs.stride[index] = stride;
  }
  else {
    attribs.buffers.bind(index,0);
    attribs.offset[index] = 0;
    attribs.client_buffer[i] = 0;
    attribs.client_buffer_enabled.reset(index);
  }
#else
  attribs.buffers.bind(index,ctx -> buffer_binding.names[RSXGL_ARRAY_BUFFER]);

  if(ctx -> buffer_binding.names[RSXGL_ARRAY_BUFFER] != 0) {
    attribs.offset[index] = rsxgl_pointer_to_offset(pointer);
    attribs.type.set(index,rsx_type);
    attribs.size.set(index,size - 1);
    attribs.stride[index] = stride;
  }
  else {
    attribs.offset[index] = 0;
  }
#endif

  ctx -> invalid_attribs.set(index);
  
  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glVertexAttribPointer (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer)
{
  uint32_t rsx_type = rsxgl_vertex_buffer_type(type,normalized);
  if(rsx_type == 0) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_vertex_attrib_pointer(current_ctx(),rsx_type,index,size,(stride == 0) ? rsxgl_vertex_buffer_stride(type) * size : stride,pointer);
}

GLAPI void APIENTRY
glVertexAttribIPointer (GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
  uint32_t rsx_type = rsxgl_vertex_buffer_type(type,GL_FALSE);
  if(rsx_type == 0 || !(rsx_type == RSXGL_VERTEX_U8_NR || rsx_type == RSXGL_VERTEX_S16_NR)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  rsxgl_vertex_attrib_pointer(current_ctx(),rsx_type,index,size,(stride == 0) ? rsxgl_vertex_buffer_stride(type) * size : stride,pointer);
}

GLAPI void APIENTRY
glVertexAttribDivisor (GLuint index, GLuint divisor)
{
  if(index >= RSXGL_MAX_VERTEX_ATTRIBS) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  rsxgl_context_t * ctx = current_ctx();
  attribs_t & attribs = ctx -> attribs_binding[0];

  // TODO: Implement this
}

void
rsxgl_attribs_validate(rsxgl_context_t * ctx,const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > & program_attribs,const uint32_t start,const uint32_t length,const uint32_t timestamp)
{
  gcmContextData * context = ctx -> base.gcm_context;

  attribs_t & attribs = ctx -> attribs_binding[0];
  const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > required_buffer_attribs = attribs.enabled & program_attribs;
  const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > invalid_attribs = ctx -> invalid_attribs & program_attribs;
#if (RSXGL_CONFIG_client_attribs == 1)
  bit_set< RSXGL_MAX_VERTEX_ATTRIBS > invalid_buffer_attribs = invalid_attribs & attribs.enabled & ~attribs.client_buffer_enabled;
  bit_set< RSXGL_MAX_VERTEX_ATTRIBS > invalid_client_buffer_attribs = attribs.enabled & attribs.client_buffer_enabled;
#else
  bit_set< RSXGL_MAX_VERTEX_ATTRIBS > invalid_buffer_attribs = invalid_attribs & attribs.enabled;
#endif
  const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > invalid_constant_attribs = invalid_attribs & ~attribs.enabled;

  // Validate the vertex cache
  // TODO: move this someplace where it can be determined if vertex buffers have been made
  // invalid, and perform the operation if that's the case.
  {
    uint32_t * buffer = gcm_reserve(context,8);

    gcm_emit_method_at(buffer,0,0x1710,1);
    gcm_emit_at(buffer,1,0);

    gcm_emit_method_at(buffer,2,NV40_3D_VTX_CACHE_INVALIDATE,1);
    gcm_emit_at(buffer,3,0);

    gcm_emit_method_at(buffer,4,NV40_3D_VTX_CACHE_INVALIDATE,1);
    gcm_emit_at(buffer,5,0);

    gcm_emit_method_at(buffer,6,NV40_3D_VTX_CACHE_INVALIDATE,1);
    gcm_emit_at(buffer,7,0);

    gcm_finish_n_commands(context,8);
  }

  // Validate modified buffers & update their timestamps:
  for(size_t i = 0;i < RSXGL_MAX_VERTEX_ATTRIBS;++i) {
    if(!required_buffer_attribs.test(i)) continue;
    if(attribs.buffers.names[i] == 0) {
      invalid_buffer_attribs.reset(i);
      continue;
    }
    rsxgl_buffer_validate(ctx,attribs.buffers[i],start,length,timestamp);
  }

  // TODO: Somewhere in here, make it so that attribs with nothing attached will disable fetching by the RSX (by setting NV30_3D_VTXFMT_SIZE to 0):
  if(invalid_buffer_attribs.any()) {
    uint32_t * buffer = gcm_reserve(context,4 * RSXGL_MAX_VERTEX_ATTRIBS);
    size_t nbuffer = 0;

    for(size_t i = 0;i < RSXGL_MAX_VERTEX_ATTRIBS;++i) {
      if(!invalid_buffer_attribs.test(i)) continue;
      const buffer_t & vbo = attribs.buffers[i];
      if(!vbo.memory) continue;

      const memory_t memory = vbo.memory + attribs.offset[i];

#if 0
      rsxgl_debug_printf("\t%u m:%u %u o:%u stride:%u size:%u t:%x\n",
			 (unsigned int)i,
			 memory.location,memory.offset,
			 attribs.offset[i],
			 (uint32_t)attribs.stride[i],
			 (uint32_t)attribs.size[i],
			 (uint32_t)attribs.type[i]);
#endif

      gcm_emit_method_at(buffer,nbuffer + 0,NV30_3D_VTXBUF(i),1);
      gcm_emit_at(buffer,nbuffer + 1,memory.offset | ((uint32_t)memory.location << 31));
      gcm_emit_method_at(buffer,nbuffer + 2,NV30_3D_VTXFMT(i),1);
      gcm_emit_at(buffer,nbuffer + 3,
		  /* ((uint32_t)attribs.frequency[i] << 16 | */
		  ((uint32_t)attribs.stride[i] << NV30_3D_VTXFMT_STRIDE__SHIFT) |
		  ((uint32_t)(attribs.size[i] + 1) << NV30_3D_VTXFMT_SIZE__SHIFT) |
		  ((uint32_t)attribs.type[i] & 0x7));
      nbuffer += 4;
    }

    gcm_finish_n_commands(context,nbuffer);
  }

#if (RSXGL_CONFIG_client_attribs == 1)
  if(invalid_client_buffer_attribs.any()) {
    for(size_t i = 0;i < RSXGL_MAX_VERTEX_ATTRIBS;++i) {
      if(!invalid_client_buffer_attribs.test(i)) continue;

      // TODO: Migrate client-side data to RSX-mapped memory.
      
    }
  }
#endif

  if(invalid_constant_attribs.any()) {
    uint32_t * buffer = gcm_reserve(context,5 * RSXGL_MAX_VERTEX_ATTRIBS);
    size_t nbuffer = 0;

    for(size_t i = 0;i < RSXGL_MAX_VERTEX_ATTRIBS;++i) {
      if(!invalid_constant_attribs.test(i)) continue;

      gcm_emit_method_at(buffer,nbuffer + 0,NV30_3D_VTX_ATTR_4F(i),4);

      gcm_emit_at(buffer,nbuffer + 1,attribs.defaults[i][0].u);
      gcm_emit_at(buffer,nbuffer + 2,attribs.defaults[i][1].u);
      gcm_emit_at(buffer,nbuffer + 3,attribs.defaults[i][2].u);
      gcm_emit_at(buffer,nbuffer + 4,attribs.defaults[i][3].u);

      nbuffer += 5;
    }

    gcm_finish_n_commands(context,nbuffer);
  }

  ctx -> invalid_attribs &= ~program_attribs;
}
