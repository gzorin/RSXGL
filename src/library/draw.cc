// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// draw.c - Geometry drawing invocation.

#include "rsxgl_config.h"

#include "gl_constants.h"
#include "rsxgl_context.h"
#include "state.h"
#include "program.h"
#include "attribs.h"
#include "uniforms.h"
#include "sync.h"
#include "timestamp.h"

#include <GL3/gl3.h>
#include "GL3/rsxgl3ext.h"
#include "error.h"

#include "gcm.h"
#include "nv40.h"
#include "gl_fifo.h"
#include "debug.h"
#include "rsxgl_assert.h"
#include "migrate.h"

#include <string.h>
#include <boost/integer/static_log2.hpp>

#if defined(GLAPI)
#undef GLAPI
#endif
#define GLAPI extern "C"

static inline uint32_t
rsxgl_draw_mode(GLenum mode)
{
  switch(mode) {
  case GL_POINTS:
    return NV30_3D_VERTEX_BEGIN_END_POINTS;
  case GL_LINES:
    return NV30_3D_VERTEX_BEGIN_END_LINES;
  case GL_LINE_LOOP:
    return NV30_3D_VERTEX_BEGIN_END_LINE_LOOP;
  case GL_LINE_STRIP:
    return NV30_3D_VERTEX_BEGIN_END_LINE_STRIP;
  case GL_TRIANGLES:
    return NV30_3D_VERTEX_BEGIN_END_TRIANGLES;
  case GL_TRIANGLE_STRIP:
    return NV30_3D_VERTEX_BEGIN_END_TRIANGLE_STRIP;
  case GL_TRIANGLE_FAN:
    return NV30_3D_VERTEX_BEGIN_END_TRIANGLE_FAN;
#if (RSXGL_CONFIG_RSX_compatibility == 1)
  case GL_QUADS_RSX:
    return NV30_3D_VERTEX_BEGIN_END_QUADS;
  case GL_QUAD_STRIP_RSX:
    return NV30_3D_VERTEX_BEGIN_END_QUAD_STRIP;
  case GL_POLYGON_RSX:
    return NV30_3D_VERTEX_BEGIN_END_POLYGON;
#endif
  default:
    RSXGL_ERROR(GL_INVALID_ENUM,~0U);
  };
}

static inline uint32_t
rsxgl_draw_elements_type(GLenum type)
{
  switch(type) {
  case GL_UNSIGNED_BYTE:
    return 2;
  case GL_UNSIGNED_SHORT:
    return NV30_3D_IDXBUF_FORMAT_TYPE_U16;
  case GL_UNSIGNED_INT:
    return NV30_3D_IDXBUF_FORMAT_TYPE_U32;
  default:
    RSXGL_ERROR(GL_INVALID_ENUM,~0U);
  };
}

// see if bound attributes are mapped - if they are, "throw" GL_INVALID_OPERATION:
static inline void
rsxgl_check_unmapped_arrays(struct rsxgl_context_t * ctx,const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > & program_attribs)
{
  attribs_t & attribs = ctx -> attribs_binding[0];

  const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > enabled_attribs = attribs.enabled & program_attribs;

  for(size_t i = 0;i < RSXGL_MAX_VERTEX_ATTRIBS;++i) {
    if(enabled_attribs.test(i) && attribs.buffers.names[i] != 0 && attribs.buffers[i].mapped) {
      RSXGL_ERROR_(GL_INVALID_OPERATION);
    }
  }
}

static inline uint32_t
rsxgl_check_draw_arrays(struct rsxgl_context_t * ctx,GLenum mode)
{
  const uint32_t rsx_primitive_type = rsxgl_draw_mode(mode);
  RSXGL_FORWARD_ERROR(~0);

  // OpenGL 3.1 manpages don't say if a GL error should be given without an active program.
  // Can't see much use in proceeding without one though.
  if(ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] == 0) {
    return ~0U;
  }

  // see if bound attributes are mapped - if they are, "throw" GL_INVALID_OPERATION:
  rsxgl_check_unmapped_arrays(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled);
  RSXGL_FORWARD_ERROR(~0);

  return rsx_primitive_type;
}

static inline uint32_t
rsxgl_draw_arrays_init(struct rsxgl_context_t * ctx,GLenum mode,const uint32_t start,const uint32_t length)
{
  const uint32_t timestamp = rsxgl_timestamp_create(ctx);

  // validate everything:
  rsxgl_state_validate(ctx);
  rsxgl_program_validate(ctx,timestamp);
  rsxgl_attribs_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled,start,length,timestamp);
  rsxgl_uniforms_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM]);
  rsxgl_textures_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM],timestamp);

  return timestamp;
}

static inline std::pair< uint32_t, uint32_t >
rsxgl_check_draw_elements(struct rsxgl_context_t * ctx,GLenum mode,GLenum type)
{
  const uint32_t rsx_primitive_type = rsxgl_draw_mode(mode);
  RSXGL_FORWARD_ERROR(std::make_pair(~0U, ~0U));

  const uint32_t rsx_type = rsxgl_draw_elements_type(type);
  if(rsx_type == ~0U) {
    RSXGL_ERROR(GL_INVALID_ENUM,std::make_pair(~0U, ~0U));
  }

  // OpenGL 3.1 manpages don't say if a GL error should be given without an active program.
  // Can't see much use in proceeding without one though.
  if(ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] == 0) {
    return std::make_pair(~0U, ~0U);
  }

  // see if bound attributes are mapped - if they are, "throw" GL_INVALID_OPERATION:
  rsxgl_check_unmapped_arrays(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled);
  RSXGL_FORWARD_ERROR(std::make_pair(~0U, ~0U));

  return std::make_pair(rsx_primitive_type,rsx_type);
}

static inline uint32_t
rsxgl_draw_elements_init(struct rsxgl_context_t * ctx,GLenum mode,GLenum type,const uint32_t start,const uint32_t length)
{
  // get a timestamp:
  const uint32_t timestamp = rsxgl_timestamp_create(ctx);

  // validate everything:
  rsxgl_state_validate(ctx);
  rsxgl_program_validate(ctx,timestamp);
  rsxgl_attribs_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled,start,length,timestamp);
  rsxgl_uniforms_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM]);
  rsxgl_textures_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM],timestamp);

  return timestamp;
}

// The bookend to the above two functions - it posts the timestamp to the command stream:
static inline void
rsxgl_draw_exit(struct rsxgl_context_t * ctx,const uint32_t timestamp)
{
  rsxgl_timestamp_post(ctx,timestamp);
}

// This function is designed to split an iteration into groups (RSX method invocations) & batches
// as required by the hardware. max_method_args is the total number of arguments accepted by a
// single "method" - this should pretty much be set to 2047. batch_size can vary depending upon
// the purpose that this function is put to. Drawing vertices & indices that have been loaded
// into arrays on the GPU, for instance, are split into batches of 256 indices; sending indices
// from client memory, over the fifo, would have a batch_size of 1.
struct rsxgl_process_batch_work_t {
  uint32_t nvertices, nbatch, nbatchremainder;

  rsxgl_process_batch_work_t(const uint32_t _nvertices, const uint32_t _nbatch, const uint32_t _nbatchremainder)
    : nvertices(_nvertices), nbatch(_nbatch), nbatchremainder(_nbatchremainder) {
  }
};

template< uint32_t max_method_args, typename Operations >
void rsxgl_process_batch(gcmContextData * context,const uint32_t n,const Operations & operations)
{
  const uint32_t batch_size = Operations::batch_size;

#if 0
  const boost::tuple< uint32_t, uint32_t, uint32_t > tmp = operations.work_info(n);
  const uint32_t actual_n = tmp.get<0>();
  const uint32_t nbatch = tmp.get<1>();
  const uint32_t nbatchremainder = tmp.get<2>();
#endif

  const rsxgl_process_batch_work_t info = operations.work_info(n);

  uint32_t ninvoc = info.nbatch / max_method_args;
  const uint32_t ninvocremainder = info.nbatch % max_method_args;

  operations.begin(context,ninvoc,ninvocremainder,info.nbatchremainder);

  operations.begin_group(1);
  operations.n_batch(0,info.nbatchremainder);
  operations.end_group(1);

  for(;ninvoc > 0;--ninvoc) {
    operations.begin_group(max_method_args);
    for(size_t i = 0;i < max_method_args;++i) {
      operations.full_batch(i);
    }
    operations.end_group(max_method_args);
  }

  if(ninvocremainder > 0) {
    operations.begin_group(ninvocremainder);
    for(size_t i = 0;i < ninvocremainder;++i) {
      operations.full_batch(i);
    }
    operations.end_group(ninvocremainder);
  }

  operations.end();
}

// You are supposed to be able to pass up to 2047 bundles of 256 batches of
// vertices to each NV30_3D_VB_VERTEX_BATCH method. Testing revealed that this number
// is apparently the much lower number of 3, and for a decent-sized mesh, it really
// ought to be one so that cache invalidation instructions can be submitted for each
// batch.
// TODO - Investigate this further.
#define RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS 1
//#define RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS 3
//#define RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS 2047

template< uint32_t max_batch_size >
struct rsxgl_draw_points {
  static const uint32_t batch_size_bits = boost::static_log2< max_batch_size >::value;
  
  struct traits {
    static const uint32_t rsx_primitive_type = NV30_3D_VERTEX_BEGIN_END_POINTS;
    static const uint32_t batch_size = max_batch_size;

    // Will have these also:
    // - repeat_offset is the amount to subtract from the start vertex index for each batch, due to 
    //   a primitive being split across the batch_size boundary (line strip, line loop, triangle strip, triangle fan, quad strip, maybe polygon, need this, potentially)
    // - repeat_first says that each batch iteration will begin by repeating the first vertex in the primitive (triangle fan, polygon need this)
    // - close_first says that the first vertex of the primitive will be repeated at the end of the entire iteration (line_loop needs this)
    // static const uint32_t repeat_offset, repeat_first, close_first;
    static const uint32_t repeat_offset = 0;

    static rsxgl_process_batch_work_t work_info(const uint32_t count) {
      return rsxgl_process_batch_work_t(count,count >> batch_size_bits,count & (batch_size - 1));
    }
  };
};

template< uint32_t max_batch_size >
struct rsxgl_draw_lines {
  static const uint32_t batch_size_bits = boost::static_log2< max_batch_size >::value;
  
  struct traits {
    static const uint32_t rsx_primitive_type = NV30_3D_VERTEX_BEGIN_END_LINES;
    static const uint32_t batch_size = max_batch_size & ~1;

    static const uint32_t repeat_offset = 0;

    static rsxgl_process_batch_work_t work_info(const uint32_t count) {
      const uint32_t _count = count & ~1;
      return rsxgl_process_batch_work_t(_count,_count >> batch_size_bits,_count & (batch_size - 1));
    }
  };
};

template< uint32_t max_batch_size >
struct rsxgl_draw_line_strip {
  struct traits {
    static const uint32_t rsx_primitive_type = NV30_3D_VERTEX_BEGIN_END_TRIANGLE_STRIP;
    // batch_size had better be pot:
    static const uint32_t batch_size = max_batch_size;
    static const uint32_t batch_size_bits = boost::static_log2< max_batch_size >::value;

    static const uint32_t repeat_offset = 1;

    static const uint32_t batch_size_minus_repeat = batch_size - repeat_offset;

    // actual number of vertices, nbatch, natch remainder:
    static rsxgl_process_batch_work_t work_info(const uint32_t count) {
      uint32_t _count = count;

      if(_count > batch_size) {
	const uint32_t tmp = _count - batch_size;
	_count = _count + (tmp / batch_size_minus_repeat * repeat_offset) + ((tmp % batch_size_minus_repeat) ? repeat_offset : 0);
      }

      return rsxgl_process_batch_work_t(batch_size,_count >> batch_size_bits,_count & (batch_size - 1));      
    }
  };
};

template< uint32_t max_batch_size >
struct rsxgl_draw_triangles {
  struct traits {
    static const uint32_t rsx_primitive_type = NV30_3D_VERTEX_BEGIN_END_TRIANGLES;
    static const uint32_t batch_size = max_batch_size - (max_batch_size % 3);

    static const uint32_t repeat_offset = 0;

    static rsxgl_process_batch_work_t work_info(const uint32_t count) {
      const uint32_t count_for_triangles = count - (count % 3);
      return rsxgl_process_batch_work_t(count_for_triangles,count_for_triangles / batch_size,count_for_triangles % batch_size);
    }
  };
};

template< uint32_t max_batch_size >
struct rsxgl_draw_triangle_strip {
  struct traits {
    static const uint32_t rsx_primitive_type = NV30_3D_VERTEX_BEGIN_END_TRIANGLE_STRIP;
    // batch_size had better be pot:
    static const uint32_t batch_size = max_batch_size;
    static const uint32_t batch_size_bits = boost::static_log2< max_batch_size >::value;

    static const uint32_t repeat_offset = 2;

    static const uint32_t batch_size_minus_repeat = batch_size - repeat_offset;

    // actual number of vertices, nbatch, natch remainder:
    static rsxgl_process_batch_work_t work_info(const uint32_t count) {
      uint32_t _count = count;

      if(_count > batch_size) {
	const uint32_t tmp = _count - batch_size;
	_count = _count + (tmp / batch_size_minus_repeat * repeat_offset) + ((tmp % batch_size_minus_repeat) ? repeat_offset : 0);
      }

      return rsxgl_process_batch_work_t(batch_size,_count >> batch_size_bits,_count & (batch_size - 1));      
    }
  };
};

template< uint32_t max_batch_size, template< uint32_t > class primitive_traits >
struct rsxgl_draw_array_operations {
  typedef typename primitive_traits< max_batch_size >::traits primitive_traits_type;
  static const uint32_t rsx_primitive_type = primitive_traits_type::rsx_primitive_type;
  static const uint32_t batch_size = primitive_traits_type::batch_size;
  static const uint32_t repeat_offset = primitive_traits_type::repeat_offset;
  
  mutable uint32_t * buffer;
  mutable uint32_t first, current;
  
  rsxgl_draw_array_operations(const uint32_t first)
    : buffer(0), first(first), current(0) {
  }

  rsxgl_process_batch_work_t work_info(const uint32_t count) const {
    return primitive_traits_type::work_info(count);
  }
  
  // ninvoc - number of full draw method invocations (2047 * 256 vertices)
  // ninvocremainder - number of vertex batches for an additional draw method invocation (ninvocremainder * 256 vertices)
  // nbatchremainder - size of one additional vertex batch (nbatchremainder vertices)
  inline void
  begin(gcmContextData * context,const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) const {
    const uint32_t nmethods = 1 + ninvoc + (ninvocremainder ? 1 : 0);
    const uint32_t nargs = 1 + (ninvoc * RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS) + ninvocremainder;
    const uint32_t nwords = nmethods + nargs + 4 + 6;
    
    buffer = gcm_reserve(context,nwords);
    current = 0;

    gcm_emit_method_at(buffer,0,NV40_3D_VTX_CACHE_INVALIDATE,1);
    gcm_emit_at(buffer,1,0);
    
    gcm_emit_method_at(buffer,2,NV40_3D_VTX_CACHE_INVALIDATE,1);
    gcm_emit_at(buffer,3,0);
    
    gcm_emit_method_at(buffer,4,NV40_3D_VTX_CACHE_INVALIDATE,1);
    gcm_emit_at(buffer,5,0);
    
    gcm_emit_method_at(buffer,6,NV30_3D_VERTEX_BEGIN_END,1);
    gcm_emit_at(buffer,7,rsx_primitive_type);

    buffer += 8;
  }
  
  // n is number of arguments to this method:
  inline void
  begin_group(const uint32_t n) const {
    gcm_emit_method_at(buffer,0,NV30_3D_VB_VERTEX_BATCH,n);
    ++buffer;
  }
  
  // n is the size of this batch (the number of vertices in this batch):
  inline void
  n_batch(const uint32_t igroup,const uint32_t n) const {
    gcm_emit_at(buffer,igroup,((n - 1) << NV30_3D_VB_VERTEX_BATCH_COUNT__SHIFT) | first + current);
    current += n - repeat_offset;
  }

  // here the size of the batch is assumed to be primitive_traits::batch_size,
  // which oughta be a constant:
  inline void
  full_batch(const uint32_t igroup) const {
    gcm_emit_at(buffer,igroup,((batch_size - 1) << NV30_3D_VB_VERTEX_BATCH_COUNT__SHIFT) | first + current);
    current += batch_size - repeat_offset;
  }
  
  inline void
  end_group(const uint32_t n) const {
    buffer += n;
  }
  
  inline void
  end() const {
    gcm_emit_method_at(buffer,0,NV30_3D_VERTEX_BEGIN_END,1);
    gcm_emit_at(buffer,1,NV30_3D_VERTEX_BEGIN_END_STOP);
    
    buffer += 2;
  }
};

static inline void
rsxgl_draw_arrays(gcmContextData * context,const uint32_t rsx_primitive_type,const uint32_t first,const uint32_t count)
{
  if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_POINTS) {
    rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_points > op(first);
    rsxgl_process_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_LINES) {
    rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_lines > op(first);
    rsxgl_process_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_LINE_STRIP) {
    rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_line_strip > op(first);
    rsxgl_process_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_TRIANGLES) {
    rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_triangles > op(first);
    rsxgl_process_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_TRIANGLE_STRIP) {
    rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_triangle_strip > op(first);
    rsxgl_process_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
}

#define RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS 1

// Operations performed by rsxgl_process_batch (a local class passed as a template argument is a C++0x feature):
template< uint32_t max_batch_size, template< uint32_t > class primitive_traits >
struct rsxgl_draw_array_elements_operations {
  typedef typename primitive_traits< max_batch_size >::traits primitive_traits_type;
  static const uint32_t rsx_primitive_type = primitive_traits_type::rsx_primitive_type;
  static const uint32_t batch_size = primitive_traits_type::batch_size;
  static const uint32_t repeat_offset = primitive_traits_type::repeat_offset;
  
  mutable uint32_t * buffer;
  mutable uint32_t current;
  
  rsxgl_draw_array_elements_operations()
    : buffer(0), current(0) {
  }

  rsxgl_process_batch_work_t work_info(const uint32_t count) const {
    return primitive_traits_type::work_info(count);
  }
  
  inline void
  begin(gcmContextData * context,const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) const {
    const uint32_t nmethods = 1 + ninvoc + (ninvocremainder ? 1 : 0);
    const uint32_t nargs = 1 + (ninvoc * RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS) + ninvocremainder;
    const uint32_t nwords = nmethods + nargs + 4;
    
    buffer = gcm_reserve(context,nwords);

    gcm_emit_method_at(buffer,0,NV30_3D_VERTEX_BEGIN_END,1);
    gcm_emit_at(buffer,1,rsx_primitive_type);

    buffer += 2;
  }
  
  // n is number of arguments to this method:
  inline void
  begin_group(const uint32_t n) const {
    gcm_emit_method_at(buffer,0,NV30_3D_VB_INDEX_BATCH,n);
    ++buffer;
  }

  // n is the size of this batch:
  inline void
  n_batch(const uint32_t igroup,const uint32_t n) const {
    gcm_emit_at(buffer,igroup,((n - 1) << NV30_3D_VB_INDEX_BATCH_COUNT__SHIFT) | current);
    current += n - repeat_offset;
  }
  
  // here the size of the batch is assumed to be primitive_traits::batch_size,
  // which oughta be a constant:
  inline void
  full_batch(const uint32_t igroup) const {
    gcm_emit_at(buffer,igroup,((batch_size - 1) << NV30_3D_VB_INDEX_BATCH_COUNT__SHIFT) | current);
    current += batch_size - repeat_offset;
  }
  
  inline void
  end_group(const uint32_t n) const {
    buffer += n;
  }
  
  inline void
  end() const {
    gcm_emit_method_at(buffer,0,NV30_3D_VERTEX_BEGIN_END,1);
    gcm_emit_at(buffer,1,NV30_3D_VERTEX_BEGIN_END_STOP);
    
    buffer += 2;
  }
};

static inline void
rsxgl_draw_array_elements(gcmContextData * context,const uint32_t rsx_primitive_type,const uint32_t count)
{
  if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_POINTS) {
    rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_points > op;
    rsxgl_process_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_LINES) {
    rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_line_strip > op;
    rsxgl_process_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_LINE_STRIP) {
    rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_line_strip > op;
    rsxgl_process_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_TRIANGLES) {
    rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_triangles > op;
    rsxgl_process_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_TRIANGLE_STRIP) {
    rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_triangle_strip > op;
    rsxgl_process_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
    context -> current = op.buffer;
  }
}

//
GLAPI void APIENTRY
glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
  struct rsxgl_context_t * ctx = current_ctx();
  gcmContextData * context = ctx -> base.gcm_context;

  const uint32_t rsx_primitive_type = rsxgl_check_draw_arrays(ctx,mode);
  RSXGL_FORWARD_ERROR_();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  // draw!
  const uint32_t timestamp = rsxgl_draw_arrays_init(ctx,mode,first,count);
  rsxgl_draw_arrays(context,rsx_primitive_type,first,count);
  rsxgl_draw_exit(ctx,timestamp);

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDrawArraysInstanced (GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
  struct rsxgl_context_t * ctx = current_ctx();
  gcmContextData * context = ctx -> base.gcm_context;

  const uint32_t rsx_primitive_type = rsxgl_check_draw_arrays(ctx,mode);
  RSXGL_FORWARD_ERROR_();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  const uint32_t timestamp = rsxgl_draw_arrays_init(ctx,mode,0,0);
}

GLAPI void APIENTRY
glMultiDrawArrays (GLenum mode, const GLint *first, const GLsizei *count, GLsizei primcount)
{
  struct rsxgl_context_t * ctx = current_ctx();
  gcmContextData * context = ctx -> base.gcm_context;

  const uint32_t rsx_primitive_type = rsxgl_check_draw_arrays(ctx,mode);
  RSXGL_FORWARD_ERROR_();

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  // draw!
  const uint32_t timestamp = rsxgl_draw_arrays_init(ctx,mode,0,0);
  for(;primcount > 0;--primcount,++first,++count) {
    rsxgl_draw_arrays(context,rsx_primitive_type,*first,*count);
  }
  rsxgl_draw_exit(ctx,timestamp);
}

//
GLAPI void APIENTRY
glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{ 
  struct rsxgl_context_t * ctx = current_ctx();

  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_();

  if(!(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] != 0 && ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER].mapped) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  // draw!
  gcmContextData * context = ctx -> base.gcm_context;

  const bool client_indices = ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] == 0;
  uint32_t migrate_buffer_size = 0;
  void * migrate_buffer = 0;

  if(client_indices) {
    const uint32_t element_size =
      (rsx_type == NV30_3D_IDXBUF_FORMAT_TYPE_U32) ? sizeof(uint32_t) : sizeof(uint16_t);
    migrate_buffer_size = element_size * count;
    migrate_buffer = rsxgl_migrate_memalign(context,16,migrate_buffer_size);
    memcpy(migrate_buffer,indices,migrate_buffer_size);
    uint32_t migrate_buffer_offset = 0;
    int32_t s = gcmAddressToOffset(migrate_buffer,&migrate_buffer_offset);
    rsxgl_assert(s == 0);
    
    //
    uint32_t * buffer = gcm_reserve(context,3);
    
    gcm_emit_method_at(buffer,0,NV30_3D_IDXBUF_OFFSET,2);
    gcm_emit_at(buffer,1,migrate_buffer_offset);
    gcm_emit_at(buffer,2,rsx_type | RSXGL_MIGRATE_BUFFER_LOCATION);
    
    gcm_finish_n_commands(context,3);
  }
  else {
    const buffer_t & index_buffer = ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER];
    const uint32_t
      offset = index_buffer.memory.offset + (uint32_t)((uint64_t)indices),
      location = index_buffer.memory.location;
    
    uint32_t * buffer = gcm_reserve(context,3);
    
    gcm_emit_method_at(buffer,0,NV30_3D_IDXBUF_OFFSET,2);
    gcm_emit_at(buffer,1,offset);
    gcm_emit_at(buffer,2,rsx_type | location);
    
    gcm_finish_n_commands(context,3);
  }

  //
  const uint32_t timestamp = rsxgl_draw_elements_init(ctx,mode,type,0,0);

  rsxgl_draw_array_elements(context,rsx_primitive_type,count);

  if(client_indices) {
    rsxgl_assert(migrate_buffer != 0);
    rsxgl_migrate_free(context,migrate_buffer,migrate_buffer_size);
  }

  rsxgl_draw_exit(ctx,timestamp);

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDrawElementsInstanced (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
  struct rsxgl_context_t * ctx = current_ctx();

  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  gcmContextData * context = ctx -> base.gcm_context;

  // TODO - Implement this
  const uint32_t timestamp = rsxgl_draw_elements_init(ctx,mode,type,0,0);
}

GLAPI void APIENTRY
glDrawElementsBaseVertex (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
  struct rsxgl_context_t * ctx = current_ctx();

  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  gcmContextData * context = ctx -> base.gcm_context;

  // TODO - Implement this:
  const uint32_t timestamp = rsxgl_draw_elements_init(ctx,mode,type,0,0);
}

GLAPI void APIENTRY
glDrawElementsInstancedBaseVertex (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex)
{
  struct rsxgl_context_t * ctx = current_ctx();

  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  gcmContextData * context = ctx -> base.gcm_context;

  // TODO - implement this
  const uint32_t timestamp = rsxgl_draw_elements_init(ctx,mode,type,0,0);
}

GLAPI void APIENTRY
glDrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
  struct rsxgl_context_t * ctx = current_ctx();

  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(end < start) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] != 0 && ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER].mapped) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  // draw!
  gcmContextData * context = ctx -> base.gcm_context;
  
  // TODO - Migrate client index arrays
  const buffer_t & index_buffer = ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER];
  const uint32_t
    offset = index_buffer.memory.offset + (uint32_t)((uint64_t)indices),
    location = index_buffer.memory.location;

  uint32_t * buffer = gcm_reserve(context,3);
  
  gcm_emit_method(&buffer,NV30_3D_IDXBUF_OFFSET,2);
  gcm_emit(&buffer,offset);
  gcm_emit(&buffer,rsx_type | location);

  gcm_finish_commands(context,&buffer);

  //
  const uint32_t timestamp = rsxgl_draw_elements_init(ctx,mode,type,start,end - start);
  rsxgl_draw_array_elements(context,rsx_primitive_type,count);
  rsxgl_draw_exit(ctx,timestamp);

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDrawRangeElementsBaseVertex (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
  struct rsxgl_context_t * ctx = current_ctx();

  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(end < start) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  gcmContextData * context = ctx -> base.gcm_context;

  // TODO - Implement this
  const uint32_t timestamp = rsxgl_draw_elements_init(ctx,mode,type,start,end - start);
}

GLAPI void APIENTRY
glMultiDrawElements (GLenum mode, const GLsizei *count, GLenum type, const GLvoid* *indices, GLsizei primcount)
{
  struct rsxgl_context_t * ctx = current_ctx();

  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_();

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] != 0 && ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER].mapped) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  gcmContextData * context = ctx -> base.gcm_context;

  const uint32_t timestamp = rsxgl_draw_elements_init(ctx,mode,type,0,0);

  // TODO - Migrate client index arrays
  const buffer_t & index_buffer = ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER];
  const uint32_t
    location = index_buffer.memory.location;

  for(;primcount > 0;--primcount,++indices,++count) {
    const uint32_t
      offset = index_buffer.memory.offset + (uint32_t)((uint64_t)*indices);

    uint32_t * buffer = gcm_reserve(context,3);
    
    gcm_emit_method_at(buffer,0,NV30_3D_IDXBUF_OFFSET,2);
    gcm_emit_at(buffer,1,offset);
    gcm_emit_at(buffer,2,rsx_type | location);
    
    gcm_finish_n_commands(context,3);
    
    rsxgl_draw_array_elements(context,rsx_primitive_type,*count);
  }

  rsxgl_draw_exit(ctx,timestamp);

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glMultiDrawElementsBaseVertex (GLenum mode, const GLsizei *count, GLenum type, const GLvoid* *indices, GLsizei primcount, const GLint *basevertex)
{
  struct rsxgl_context_t * ctx = current_ctx();

  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_();

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  gcmContextData * context = ctx -> base.gcm_context;

  const uint32_t timestamp = rsxgl_draw_elements_init(ctx,mode,type,0,0);
}

GLAPI void APIENTRY
glPrimitiveRestartIndex (GLuint index)
{
  struct rsxgl_context_t * ctx = current_ctx();
  ctx -> state.primitiveRestartIndex = index;
  ctx -> state.invalid.parts.primitive_restart = 1;
}
