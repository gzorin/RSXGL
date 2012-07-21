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
#include "draw.h"

#include <GL3/gl3.h>
#include "GL3/rsxgl3ext.h"
#include "error.h"

#include <rsx/gcm_sys.h>
#include "nv40.h"
#include "gl_fifo.h"
#include "debug.h"
#include "rsxgl_assert.h"
#include "migrate.h"

#include <string.h>
#include <boost/integer/static_log2.hpp>
#include <algorithm>

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
  case GL_UNSIGNED_INT:
    return RSXGL_ELEMENT_TYPE_UNSIGNED_INT /* NV30_3D_IDXBUF_FORMAT_TYPE_U32 */;
  case GL_UNSIGNED_SHORT:
    return RSXGL_ELEMENT_TYPE_UNSIGNED_SHORT /* NV30_3D_IDXBUF_FORMAT_TYPE_U16 */;
  case GL_UNSIGNED_BYTE:
    return RSXGL_ELEMENT_TYPE_UNSIGNED_BYTE /* 2 */;
  default:
    RSXGL_ERROR(GL_INVALID_ENUM,RSXGL_MAX_ELEMENT_TYPES);
  };
}

// see if bound attributes are mapped - if they are, "throw" GL_INVALID_OPERATION:
static inline void
rsxgl_check_unmapped_arrays(rsxgl_context_t * ctx,const bit_set< RSXGL_MAX_VERTEX_ATTRIBS > & program_attribs)
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
rsxgl_check_draw_arrays(rsxgl_context_t * ctx,GLenum mode)
{
  const uint32_t rsx_primitive_type = rsxgl_draw_mode(mode);
  RSXGL_FORWARD_ERROR(~0);

  // OpenGL 3.1 manpages don't say if a GL error should be given without an active program.
  // Can't see much use in proceeding without one though.
  if(ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] == 0 || !ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].linked) {
    return ~0U;
  }

  // see if bound attributes are mapped - if they are, "throw" GL_INVALID_OPERATION:
  rsxgl_check_unmapped_arrays(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled);
  RSXGL_FORWARD_ERROR(~0);

  return rsx_primitive_type;
}

static inline std::pair< uint32_t, uint32_t >
rsxgl_check_draw_elements(rsxgl_context_t * ctx,GLenum mode,GLenum type)
{
  const uint32_t rsx_primitive_type = rsxgl_draw_mode(mode);
  RSXGL_FORWARD_ERROR(std::make_pair(~0U, RSXGL_MAX_ELEMENT_TYPES));

  const uint32_t rsx_type = rsxgl_draw_elements_type(type);
  if(rsx_type == RSXGL_MAX_ELEMENT_TYPES) {
    RSXGL_ERROR(GL_INVALID_ENUM,std::make_pair(~0U,RSXGL_MAX_ELEMENT_TYPES));
  }

  // OpenGL 3.1 manpages don't say if a GL error should be given without an active program.
  // Can't see much use in proceeding without one though.
  if(ctx -> program_binding.names[RSXGL_ACTIVE_PROGRAM] == 0 || !ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].linked) {
    return std::make_pair(~0U, RSXGL_MAX_ELEMENT_TYPES);
  }

  // see if bound attributes are mapped - if they are, "throw" GL_INVALID_OPERATION:
  rsxgl_check_unmapped_arrays(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled);
  RSXGL_FORWARD_ERROR(std::make_pair(~0U, RSXGL_MAX_ELEMENT_TYPES));

  return std::make_pair(rsx_primitive_type,rsx_type);
}

static inline uint32_t
rsxgl_draw_init(rsxgl_context_t * ctx,GLenum mode,const uint32_t start,const uint32_t length,const uint32_t count)
{
  const uint32_t timestamp = rsxgl_timestamp_create(ctx,count);
  const uint32_t last_timestamp = timestamp + count - 1;

  // validate everything:
  rsxgl_draw_framebuffer_validate(ctx,last_timestamp);
  rsxgl_state_validate(ctx);
  rsxgl_program_validate(ctx,last_timestamp);
  rsxgl_attribs_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled,start,length,last_timestamp);
  rsxgl_uniforms_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM]);
  rsxgl_textures_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM],last_timestamp);

  return timestamp;
}

struct rsxgl_draw_elements_info_t {
  const bool client_indices;
  const void * migrate_buffer;
  const uint32_t migrate_buffer_size;

  rsxgl_draw_elements_info_t(const bool _client_indices,void const * _migrate_buffer,const uint32_t _migrate_buffer_size)
    : client_indices(_client_indices), migrate_buffer(_migrate_buffer), migrate_buffer_size(_migrate_buffer_size) {
  }
};

#define NV30_3D_IDXBUF_FORMAT_TYPE_U8 2

static const uint8_t rsxgl_element_nv40_type[RSXGL_MAX_ELEMENT_TYPES] = {
  NV30_3D_IDXBUF_FORMAT_TYPE_U32,
  NV30_3D_IDXBUF_FORMAT_TYPE_U16,
  NV30_3D_IDXBUF_FORMAT_TYPE_U8
};

#undef NV30_3D_IDXBUF_FORMAT_TYPE_U8

static const uint8_t rsxgl_element_type_bytes[RSXGL_MAX_ELEMENT_TYPES] = {
  sizeof(uint32_t),
  sizeof(uint16_t),
  sizeof(uint8_t)
};

static inline void
rsxgl_draw_elements_emit_index_buffer(gcmContextData * context,const uint32_t rsx_type,const uint32_t offset,const uint32_t location)
{
  uint32_t * buffer = gcm_reserve(context,3);
  
  gcm_emit_method_at(buffer,0,NV30_3D_IDXBUF_OFFSET,2);
  gcm_emit_at(buffer,1,offset);
  gcm_emit_at(buffer,2,(uint32_t)rsxgl_element_nv40_type[rsx_type] | location);
  
  gcm_finish_n_commands(context,3);
}

static inline rsxgl_draw_elements_info_t
rsxgl_draw_elements_init(rsxgl_context_t * ctx,const GLuint start,const GLuint end,const GLsizei count,const uint32_t rsx_type,const GLvoid * indices,const uint32_t timestamp)
{
  gcmContextData * context = ctx -> gcm_context();

  const bool client_indices = ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] == 0;
  void * migrate_buffer = 0;

  uint32_t index_buffer_offset = 0, index_buffer_location = 0;
  const uint32_t index_buffer_size = (uint32_t)rsxgl_element_type_bytes[rsx_type] * count;

  if(client_indices) {
    migrate_buffer = rsxgl_vertex_migrate_memalign(context,16,index_buffer_size);
    memcpy(migrate_buffer,indices,index_buffer_size);
    int32_t s = gcmAddressToOffset(migrate_buffer,&index_buffer_offset);
    rsxgl_assert(s == 0);

    index_buffer_location = RSXGL_VERTEX_MIGRATE_BUFFER_LOCATION;
  }
  else {
    const uint32_t start = (uint32_t)((uint64_t)indices);
    rsxgl_buffer_validate(ctx,ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER],start,index_buffer_size,timestamp);

    const buffer_t & index_buffer = ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER];
    index_buffer_offset = index_buffer.memory.offset + start;
    index_buffer_location = index_buffer.memory.location;
  }

  rsxgl_draw_elements_emit_index_buffer(context,rsx_type,index_buffer_offset,index_buffer_location);

  return rsxgl_draw_elements_info_t(client_indices,migrate_buffer,index_buffer_size);
}

static inline void
rsxgl_draw_elements_exit(rsxgl_context_t * ctx,rsxgl_draw_elements_info_t const & info)
{
  gcmContextData * context = ctx -> gcm_context();

  if(info.client_indices) {
    rsxgl_assert(info.migrate_buffer != 0);
    rsxgl_vertex_migrate_free(context,info.migrate_buffer,info.migrate_buffer_size);
  }
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
uint32_t rsxgl_count_batch(const uint32_t n)
{
  const uint32_t batch_size = Operations::batch_size;
  const rsxgl_process_batch_work_t info = Operations::work_info(n);
  const uint32_t ninvoc = info.nbatch / max_method_args;
  const uint32_t ninvocremainder = info.nbatch % max_method_args;

  return Operations::count(ninvoc,ninvocremainder,info.nbatchremainder);
}

template< uint32_t max_method_args, typename Operations >
void rsxgl_process_batch(gcmContextData * context,const uint32_t n,const Operations & operations)
{
  const uint32_t batch_size = Operations::batch_size;

  const rsxgl_process_batch_work_t info = Operations::work_info(n);

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
// TODO: Investigate this further.
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

  static inline rsxgl_process_batch_work_t
  work_info(const uint32_t count) {
    return primitive_traits_type::work_info(count);
  }

  static inline uint32_t
  count(const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) {
    const uint32_t nmethods = 1 + ninvoc + (ninvocremainder ? 1 : 0);
    const uint32_t nargs = 1 + (ninvoc * RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS) + ninvocremainder;
    const uint32_t nwords = nmethods + nargs + 4 + 6;

    return nwords;
  }
  
  // ninvoc - number of full draw method invocations (2047 * 256 vertices)
  // ninvocremainder - number of vertex batches for an additional draw method invocation (ninvocremainder * 256 vertices)
  // nbatchremainder - size of one additional vertex batch (nbatchremainder vertices)
  inline void
  begin(gcmContextData * context,const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) const {
    buffer = gcm_reserve(context,count(ninvoc,ninvocremainder,nbatchremainder));

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

static inline uint32_t
rsxgl_count_arrays(const uint32_t rsx_primitive_type,const uint32_t count)
{
  if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_POINTS) {
    return rsxgl_count_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_points > > (count);
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_LINES) {
    return rsxgl_count_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_line_strip > > (count);
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_LINE_STRIP) {
    return rsxgl_count_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_line_strip > > (count);
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_TRIANGLES) {
    return rsxgl_count_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_triangles > > (count);
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_TRIANGLE_STRIP) {
    return rsxgl_count_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_triangle_strip > > (count);
  }
  else {
    return 0;
  }
}

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

  static inline rsxgl_process_batch_work_t
  work_info(const uint32_t count) {
    return primitive_traits_type::work_info(count);
  }

  static inline uint32_t
  count(const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) {
    const uint32_t nmethods = 1 + ninvoc + (ninvocremainder ? 1 : 0);
    const uint32_t nargs = 1 + (ninvoc * RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS) + ninvocremainder;
    const uint32_t nwords = nmethods + nargs + 4;

    return nwords;
  }
  
  inline void
  begin(gcmContextData * context,const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) const {
    buffer = gcm_reserve(context,count(ninvoc,ninvocremainder,nbatchremainder));

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

static inline uint32_t
rsxgl_count_array_elements(const uint32_t rsx_primitive_type,const uint32_t count)
{
  if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_POINTS) {
    return rsxgl_count_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_points > > (count);
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_LINES) {
    return rsxgl_count_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_line_strip > > (count);
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_LINE_STRIP) {
    return rsxgl_count_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_line_strip > > (count);
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_TRIANGLES) {
    return rsxgl_count_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_triangles > > (count);
  }
  else if(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_TRIANGLE_STRIP) {
    return rsxgl_count_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS, rsxgl_draw_array_elements_operations< RSXGL_MAX_DRAW_BATCH_SIZE, rsxgl_draw_triangle_strip > > (count);
  }
  else {
    return 0;
  }
}

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

static inline void
rsxgl_draw_array_elements_base(gcmContextData * context,const uint32_t base)
{
  uint32_t * buffer = gcm_reserve(context,2);
  gcm_emit_method_at(buffer,0,0x173c,1);
  gcm_emit_at(buffer,1,base);
  gcm_finish_n_commands(context,2);
}

#if 0
// Tags for draw behavior:
namespace {
  namespace draw_tags {
    // How many primitives?
    struct single {};
    struct multi {};
    struct instanced {};

    // Arrays or elements?
    struct arrays {};
    struct elements {};

    // How to obtain element range?
    struct no_range {};
    struct scan_range {};
    struct explicit_range {};

    // How to handle base vertex?
    struct no_base {};
    struct zero_base {};
    struct explicit_base {};
  };
};

namespace {

  template< typename SingleOrMulti, typename ArrayOrElements, typename Range, typename Base >
  struct draw_functions;

  struct draw_functions_base {
    gcmContextData * const context;
    const uint32_t rsx_primitive_type;

    draw_functions_base(gcmContextData * _context,const uint32_t _rsx_primitive_type)
      : context(_context), rsx_primitive_type(_rsx_primitive_type) {
    }
  };

  template< >
  struct draw_functions< draw_tags::single, draw_tags::arrays, draw_tags::no_range, draw_tags::no_base > : public draw_functions_base {
    const GLint * const first;
    const GLsizei * const count;

    draw_functions(rsxgl_context_t * _ctx,
		   const uint32_t _rsx_primitive_type,
		   const GLsizei _primitive_count,const GLint * _first,const GLsizei * _count,
		   const uint32_t _rsx_element_type,const GLint _element_base,const GLuint _min_element,const GLuint _max_element,const GLvoid ** _element_indices)
      : draw_functions_base(_ctx -> gcm_context(), _rsx_primitive_type),
	first(_first), count(_count) {
    }

    std::pair< uint32_t, uint32_t > index_range() {
      return std::make_pair(*first,*count);
    }

    size_t iteration_count() {
      return 1;
    }

    void start() {}

    void iteration(const size_t) {
      rsxgl_draw_arrays(context,rsx_primitive_type,*first,*count);
    }

    void finish() {}
  };

  template< >
  struct draw_functions< draw_tags::multi, draw_tags::arrays, draw_tags::no_range, draw_tags::no_base > : public draw_functions_base {
    const GLsizei primitive_count;
    const GLint * const first;
    const GLsizei * const count;
    
    const rsxgl_query_object_index_type query_index;
    
    draw_functions(rsxgl_context_t * _ctx,
		   const uint32_t _rsx_primitive_type,
		   const GLsizei _primitive_count,const GLint * _first,const GLsizei * _count,
		   const uint32_t _rsx_element_type,const GLint _element_base,const GLuint _min_element,const GLuint _max_element,const GLvoid ** _element_indices)
      : draw_functions_base(_ctx -> gcm_context(),_rsx_primitive_type),
	primitive_count(_primitive_count), first(_first), count(_count),
	query_index(_ctx -> any_samples_passed_query) {
    }

    std::pair< uint32_t, uint32_t > index_range() {
      const GLint * min_first = std::min_element(first,first + primitive_count);
      const GLsizei * max_count = std::max_element(count,count + primitive_count);
      return std::make_pair(*min_first,*max_count);
    }

    size_t iteration_count() {
      return primitive_count;
    }

    void start() {}

    void iteration(const size_t i) {
      rsxgl_draw_arrays(context,rsx_primitive_type,first[i],count[i]);
      if(query_index != RSXGL_MAX_QUERY_OBJECTS) {
	rsxgl_query_object_set(context,query_index);
      }
    }

    void finish() {}
  };

};

template< typename SingleOrMulti, typename ArrayOrElements, typename Range, typename Base >
static inline void
rsxgl_draw(rsxgl_context_t * ctx,
	   const uint32_t rsx_primitive_type,
	   const GLsizei primitive_count,const GLint * first,const GLsizei * count,
	   const uint32_t rsx_element_type,const GLint element_base,const GLuint min_element,const GLuint max_element,const GLvoid ** element_indices)
{
  rsxgl_debug_printf("%s\n",__PRETTY_FUNCTION__);

  typedef draw_functions< SingleOrMulti, ArrayOrElements, Range, Base > draw_functions_type;
  draw_functions_type functions(ctx,rsx_primitive_type,primitive_count,first,count,rsx_element_type,element_base,min_element,max_element,element_indices);

  // Compute the range of array elements used by this draw call:
  const std::pair< uint32_t, uint32_t > index_range = functions.index_range();

  // Number of iterations:
  const size_t iteration_count = functions.iteration_count();

  // - always compute timestamp(s):
  uint32_t timestamp = rsxgl_timestamp_create(ctx,iteration_count);
  const uint32_t last_timestamp = timestamp + iteration_count - 1;

  // validate everything:
  // - always validate state, attribs, etc.
  rsxgl_draw_framebuffer_validate(ctx,last_timestamp);
  rsxgl_state_validate(ctx);
  rsxgl_program_validate(ctx,last_timestamp);
  rsxgl_attribs_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled,index_range.first,index_range.second,last_timestamp);
  rsxgl_uniforms_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM]);
  rsxgl_textures_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM],last_timestamp);

  // - maybe migrate client indices
  // - maybe set base vertex
  functions.start();

  // Iterate:
  // - for each iteration:
  //   - on first, maybe cache as "display list"
  //   - maybe set base vertex
  //   - maybe update query object
  //   - post timestamp
  for(size_t i = 0;i < iteration_count;++i,++timestamp) {
    functions.iteration(i);
    rsxgl_timestamp_post(ctx,timestamp);
  }

  // Exit:
  // - maybe free client indices
  // - maybe reset base vertex
  functions.finish();
}
#endif

namespace {
  template< typename ElementRangePolicy, typename IterationPolicy, typename DrawPolicy >
  void rsxgl_draw(rsxgl_context_t * ctx,const ElementRangePolicy & elementRangePolicy,const IterationPolicy & iterationPolicy,const DrawPolicy & drawPolicy)
  {
    rsxgl_debug_printf("%s\n",__PRETTY_FUNCTION__);

    // Compute the range of array elements used by this draw call:
    const std::pair< uint32_t, uint32_t > index_range = elementRangePolicy.range();

    // Iteration:
    typename IterationPolicy::iterator it = iterationPolicy.begin(), it_end = iterationPolicy.end();

    // Timestamps, determined by the number of iterations:
    const size_t timestampCount = it_end - it;
    uint32_t timestamp = rsxgl_timestamp_create(ctx,timestampCount);
    const uint32_t lastTimestamp = timestamp + timestampCount - 1;

    // Validate state:
    rsxgl_draw_framebuffer_validate(ctx,lastTimestamp);
    rsxgl_state_validate(ctx);
    rsxgl_program_validate(ctx,lastTimestamp);
    rsxgl_attribs_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].attribs_enabled,index_range.first,index_range.second,lastTimestamp);
    rsxgl_uniforms_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM]);
    rsxgl_textures_validate(ctx,ctx -> program_binding[RSXGL_ACTIVE_PROGRAM],lastTimestamp);

    // Draw functions:
    gcmContextData * gcm_context = ctx -> gcm_context();

    drawPolicy.begin(gcm_context);
    for(;it != it_end;++it,++timestamp) {
      drawPolicy.draw(gcm_context,it);
      rsxgl_timestamp_post(ctx,timestamp);
    }
    drawPolicy.end(gcm_context);
  }

  struct single_iteration_policy {
    typedef unsigned int iterator;
    
    iterator begin() const {
      return 0;
    }
    
    iterator end() const {
      return 1;
    }
  };

  struct multi_iteration_policy {
    typedef unsigned int iterator;
    
    GLsizei primcount;
    
    multi_iteration_policy(GLsizei _primcount) : primcount(_primcount) {}
    
    iterator begin() const {
      return 0;
    }
    
    iterator end() const {
      return primcount;
    }
  };

  struct array_draw_policy {
    const uint32_t rsx_primitive_type;
    
    array_draw_policy(uint32_t _rsx_primitive_type)
      : rsx_primitive_type(_rsx_primitive_type) {
    }
    
    void begin(gcmContextData *) const {}
    void end(gcmContextData *) const {}
  };
}

//
GLAPI void APIENTRY
glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  const uint32_t rsx_primitive_type = rsxgl_check_draw_arrays(ctx,mode);
  RSXGL_FORWARD_ERROR_END();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    struct element_range_policy {
      const GLint first;
      const GLsizei count;

      element_range_policy(GLint _first,GLint _count) : first(_first), count(_count) {}

      std::pair< uint32_t, uint32_t > range() const {
	return std::pair< uint32_t, uint32_t >(first,count);
      }
    };

    struct draw_policy : public array_draw_policy {
      const GLint first;
      const GLsizei count;

      draw_policy(uint32_t _rsx_primitive_type,GLint _first,GLint _count)
	: array_draw_policy(_rsx_primitive_type), first(_first), count(_count) {
      }

      void draw(gcmContextData * gcm_context,unsigned int) const {
	rsxgl_draw_arrays(gcm_context,rsx_primitive_type,first,count);
      }
    };

    rsxgl_draw(ctx,element_range_policy(first,count),single_iteration_policy(),draw_policy(rsx_primitive_type,first,count));
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glMultiDrawArrays (GLenum mode, const GLint *first, const GLsizei *count, const GLsizei primcount)
{
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  const uint32_t rsx_primitive_type = rsxgl_check_draw_arrays(ctx,mode);
  RSXGL_FORWARD_ERROR_END();

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    //rsxgl_draw< draw_tags::multi, draw_tags::arrays, draw_tags::no_range, draw_tags::no_base >(ctx,rsx_primitive_type,primcount,first,count,0,0,0,0,0);

    struct element_range_policy {
      const GLint * first;
      const GLsizei * count;
      const GLsizei primcount;

      element_range_policy(const GLint * _first,const GLint * _count,GLsizei _primcount) : first(_first), count(_count), primcount(_primcount) {}

      std::pair< uint32_t, uint32_t > range() const {
	return std::pair< uint32_t, uint32_t >(*std::min_element(first,first + primcount),*std::max_element(count,count + primcount));
      }
    };

    struct draw_policy : public array_draw_policy {
      const GLint * first;
      const GLsizei * count;
      const rsxgl_query_object_index_type query_index;
      const bool set_query_object;

      draw_policy(uint32_t _rsx_primitive_type,const GLint * _first,const GLint * _count,rsxgl_query_object_index_type _query_index)
	: array_draw_policy(_rsx_primitive_type), first(_first), count(_count), query_index(_query_index), set_query_object(query_index != RSXGL_MAX_QUERY_OBJECTS) {
      }

      void draw(gcmContextData * gcm_context,unsigned int i) const {
	rsxgl_draw_arrays(gcm_context,rsx_primitive_type,first[i],count[i]);
	if(set_query_object) {
	  rsxgl_query_object_set(gcm_context,query_index);
	}
      }
    };

    rsxgl_draw(ctx,element_range_policy(first,count,primcount),multi_iteration_policy(primcount),draw_policy(rsx_primitive_type,first,count,ctx -> any_samples_passed_query));
  }
}

//
GLAPI void APIENTRY
glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{ 
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_END();

  if(!(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT)) {
    RSXGL_ERROR_(GL_INVALID_ENUM);
  }

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] != 0 && ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER].mapped) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    // draw!
    gcmContextData * context = ctx -> gcm_context();
    
    //
    const uint32_t timestamp = rsxgl_draw_init(ctx,mode,0,0,1);
    const rsxgl_draw_elements_info_t info = rsxgl_draw_elements_init(ctx,0,0,count,rsx_type,indices,timestamp);
    rsxgl_draw_array_elements(context,rsx_primitive_type,count);
    rsxgl_timestamp_post(ctx,timestamp);
    rsxgl_draw_elements_exit(ctx,info);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDrawElementsBaseVertex (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_END();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] != 0 && ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER].mapped) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    gcmContextData * context = ctx -> gcm_context();
    
    const uint32_t timestamp = rsxgl_draw_init(ctx,mode,0,0,1);
    const rsxgl_draw_elements_info_t info = rsxgl_draw_elements_init(ctx,0,0,count,rsx_type,indices,timestamp);
    rsxgl_draw_array_elements_base(context,basevertex);
    rsxgl_draw_array_elements(context,rsx_primitive_type,count);
    rsxgl_timestamp_post(ctx,timestamp);
    rsxgl_draw_array_elements_base(context,0);
    rsxgl_draw_elements_exit(ctx,info);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_END();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(end < start) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] != 0 && ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER].mapped) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    // draw!
    gcmContextData * context = ctx -> gcm_context();
    
    const uint32_t timestamp = rsxgl_draw_init(ctx,mode,start,end - start,1);    
    const rsxgl_draw_elements_info_t info = rsxgl_draw_elements_init(ctx,start,end,count,rsx_type,indices,timestamp);
    rsxgl_draw_array_elements(context,rsx_primitive_type,count);
    rsxgl_timestamp_post(ctx,timestamp);
    rsxgl_draw_elements_exit(ctx,info);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDrawRangeElementsBaseVertex (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_END();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(end < start) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] != 0 && ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER].mapped) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    gcmContextData * context = ctx -> gcm_context();
    
    const uint32_t timestamp = rsxgl_draw_init(ctx,mode,start,end - start,1);    
    const rsxgl_draw_elements_info_t info = rsxgl_draw_elements_init(ctx,start,end,count,rsx_type,indices,timestamp);
    rsxgl_draw_array_elements_base(context,basevertex);
    rsxgl_draw_array_elements(context,rsx_primitive_type,count);
    rsxgl_timestamp_post(ctx,timestamp);
    rsxgl_draw_array_elements_base(context,0);
    rsxgl_draw_elements_exit(ctx,info);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glMultiDrawElements (const GLenum mode, const GLsizei *count, GLenum type, const GLvoid* *indices, const GLsizei primcount)
{
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_END();

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] != 0 && ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER].mapped) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    gcmContextData * context = ctx -> gcm_context();
    
    uint32_t timestamp = rsxgl_draw_init(ctx,mode,0,0,primcount);
    
    const rsxgl_query_object_index_type query_index = ctx -> any_samples_passed_query;
    
    const bool client_indices = ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] == 0;
    
    if(client_indices) {
      void * migrate_buffers[primcount];
      uint32_t index_buffer_sizes[primcount];
      uint32_t index_buffer_offsets[primcount];
      for(GLsizei i = 0;i < primcount;++i) {
	index_buffer_sizes[i] = (uint32_t)rsxgl_element_type_bytes[rsx_type] * count[i];
	migrate_buffers[i] = rsxgl_vertex_migrate_memalign(context,16,index_buffer_sizes[i]);
	memcpy(migrate_buffers[i],indices,index_buffer_sizes[i]);
	int32_t s = gcmAddressToOffset(migrate_buffers[i],index_buffer_offsets + i);
	rsxgl_assert(s == 0);
      }
      
      if(query_index == RSXGL_MAX_QUERY_OBJECTS) {
	for(GLsizei i = 0;i < primcount;++i) {
	  rsxgl_draw_elements_emit_index_buffer(context,rsx_type,index_buffer_offsets[i],RSXGL_VERTEX_MIGRATE_BUFFER_LOCATION);
	  rsxgl_draw_array_elements(context,rsx_primitive_type,count[i]);
	}
	rsxgl_timestamp_post(ctx,timestamp + primcount - 1);
      }
      else {
	for(GLsizei i = 0;i < primcount;++i) {
	  rsxgl_draw_elements_emit_index_buffer(context,rsx_type,index_buffer_offsets[i],RSXGL_VERTEX_MIGRATE_BUFFER_LOCATION);
	  rsxgl_draw_array_elements(context,rsx_primitive_type,count[i]);
	  rsxgl_query_object_set(context,query_index);
	  rsxgl_timestamp_post(ctx,timestamp++);
	}
      }
      
      for(GLsizei i = 0;i < primcount;++i) {
	rsxgl_vertex_migrate_free(context,migrate_buffers[i],index_buffer_sizes[i]);
      }
    }
    else {
      uint32_t min_start = std::numeric_limits< uint32_t >::max(), max_length = std::numeric_limits< uint32_t >::min();
      for(GLsizei i = 0;i < primcount;++i) {
	min_start = std::min(min_start,(uint32_t)((uint64_t)indices[i]));
	max_length = std::max(max_length,(uint32_t)count[i]);
      }
      rsxgl_buffer_validate(ctx,ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER],min_start,max_length,timestamp + primcount - 1);
      
      const buffer_t & index_buffer = ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER];
      const uint32_t
	location = index_buffer.memory.location;
      
      if(query_index == RSXGL_MAX_QUERY_OBJECTS) {
	for(GLsizei i = 0;i < primcount;++i) {
	  rsxgl_draw_elements_emit_index_buffer(context,rsx_type,index_buffer.memory.offset + (uint32_t)((uint64_t)indices[i]),location);
	  rsxgl_draw_array_elements(context,rsx_primitive_type,count[i]);
	}
	rsxgl_timestamp_post(ctx,timestamp + primcount - 1);
      }
      else {
	for(GLsizei i = 0;i < primcount;++i) {
	  rsxgl_draw_elements_emit_index_buffer(context,rsx_type,index_buffer.memory.offset + (uint32_t)((uint64_t)indices[i]),location);
	  rsxgl_draw_array_elements(context,rsx_primitive_type,count[i]);
	  rsxgl_timestamp_post(ctx,timestamp++);
	}
      }
    }
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glMultiDrawElementsBaseVertex (GLenum mode, const GLsizei *count, GLenum type, const GLvoid* *indices, GLsizei primcount, const GLint *basevertex)
{
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_END();

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] != 0 && ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER].mapped) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    gcmContextData * context = ctx -> gcm_context();
    
    const rsxgl_query_object_index_type query_index = ctx -> any_samples_passed_query;
    const bool client_indices = ctx -> buffer_binding.names[RSXGL_ELEMENT_ARRAY_BUFFER] == 0;

    uint32_t timestamp = rsxgl_draw_init(ctx,mode,0,0,primcount);
    
    if(client_indices) {
      void * migrate_buffers[primcount];
      uint32_t index_buffer_sizes[primcount];
      uint32_t index_buffer_offsets[primcount];
      for(GLsizei i = 0;i < primcount;++i) {
	index_buffer_sizes[i] = (uint32_t)rsxgl_element_type_bytes[rsx_type] * count[i];
	migrate_buffers[i] = rsxgl_vertex_migrate_memalign(context,16,index_buffer_sizes[i]);
	memcpy(migrate_buffers[i],indices,index_buffer_sizes[i]);
	int32_t s = gcmAddressToOffset(migrate_buffers[i],index_buffer_offsets + i);
	rsxgl_assert(s == 0);
      }
      
      if(query_index == RSXGL_MAX_QUERY_OBJECTS) {
	for(GLsizei i = 0;i < primcount;++i) {
	  rsxgl_draw_elements_emit_index_buffer(context,rsx_type,index_buffer_offsets[i],RSXGL_VERTEX_MIGRATE_BUFFER_LOCATION);
	  rsxgl_draw_array_elements_base(context,basevertex[i]);
	  rsxgl_draw_array_elements(context,rsx_primitive_type,count[i]);
	}
	rsxgl_timestamp_post(ctx,timestamp + primcount - 1);
      }
      else {
	for(GLsizei i = 0;i < primcount;++i) {
	  rsxgl_draw_elements_emit_index_buffer(context,rsx_type,index_buffer_offsets[i],RSXGL_VERTEX_MIGRATE_BUFFER_LOCATION);
	  rsxgl_draw_array_elements_base(context,basevertex[i]);
	  rsxgl_draw_array_elements(context,rsx_primitive_type,count[i]);
	  rsxgl_query_object_set(context,query_index);
	  rsxgl_timestamp_post(ctx,timestamp++);
	}
      }
      
      rsxgl_draw_array_elements_base(context,0);
      
      for(GLsizei i = 0;i < primcount;++i) {
	rsxgl_vertex_migrate_free(context,migrate_buffers[i],index_buffer_sizes[i]);
      }
    }
    else {
      uint32_t min_start = std::numeric_limits< uint32_t >::max(), max_length = std::numeric_limits< uint32_t >::min();
      for(GLsizei i = 0;i < primcount;++i) {
	min_start = std::min(min_start,(uint32_t)((uint64_t)indices[i]));
	max_length = std::max(max_length,(uint32_t)count[i]);
      }
      rsxgl_buffer_validate(ctx,ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER],min_start,max_length,timestamp + primcount - 1);
      
      const buffer_t & index_buffer = ctx -> buffer_binding[RSXGL_ELEMENT_ARRAY_BUFFER];
      const uint32_t
	location = index_buffer.memory.location;
      
      if(query_index == RSXGL_MAX_QUERY_OBJECTS) {
	for(GLsizei i = 0;i < primcount;++i) {
	  rsxgl_draw_elements_emit_index_buffer(context,rsx_type,index_buffer.memory.offset + (uint32_t)((uint64_t)indices[i]),location);
	  rsxgl_draw_array_elements_base(context,basevertex[i]);
	  rsxgl_draw_array_elements(context,rsx_primitive_type,count[i]);
	}
	rsxgl_timestamp_post(ctx,timestamp + primcount - 1);
      }
      else {    
	for(GLsizei i = 0;i < primcount;++i) {
	  rsxgl_draw_elements_emit_index_buffer(context,rsx_type,index_buffer.memory.offset + (uint32_t)((uint64_t)indices[i]),location);
	  rsxgl_draw_array_elements_base(context,basevertex[i]);
	  rsxgl_draw_array_elements(context,rsx_primitive_type,count[i]);
	  
	  rsxgl_timestamp_post(ctx,timestamp++);
	}
      }
      
      rsxgl_draw_array_elements_base(context,0);
    }
  }

  RSXGL_NOERROR_();
}

struct rsxgl_instance_info_t {
  uint32_t call_offset, jump_offset;

  rsxgl_instance_info_t(const uint32_t _call_offset,const uint32_t _jump_offset)
    : call_offset(_call_offset), jump_offset(_jump_offset) {
  }
};

static inline rsxgl_instance_info_t
rsxgl_instance_draw_init(gcmContextData * context,const uint32_t nwords)
{
  gcm_reserve(context,nwords + 2);

  // call location - current position + 1
  // jump location - current position + 1 word + nwords + 1 word
  uint32_t call_offset = 0, jump_offset = 0;
  int32_t s = 0;
  
  s = gcmAddressToOffset(context -> current + 1,&call_offset);
  rsxgl_assert(s == 0);
  
  s = gcmAddressToOffset(context -> current + nwords + 2,&jump_offset);
  rsxgl_assert(s == 0);
  
  return rsxgl_instance_info_t(call_offset,jump_offset);
}

// returns call_offset
static inline uint32_t
rsxgl_draw_arrays_instanced(gcmContextData * context,const uint32_t rsx_primitive_type,const uint32_t first,const uint32_t count)
{
  const rsxgl_instance_info_t info = rsxgl_instance_draw_init(context,rsxgl_count_arrays(rsx_primitive_type,count));

  gcm_emit_at(context -> current,0,gcm_jump_cmd(info.jump_offset)); ++context -> current;
  rsxgl_draw_arrays(context,rsx_primitive_type,first,count);
  gcm_emit_at(context -> current,0,gcm_return_cmd()); ++context -> current;

  return info.call_offset;
}

static inline uint32_t
rsxgl_draw_array_elements_instanced(gcmContextData * context,const uint32_t rsx_primitive_type,const uint32_t count)
{
  const rsxgl_instance_info_t info = rsxgl_instance_draw_init(context,rsxgl_count_array_elements(rsx_primitive_type,count));

  gcm_emit_at(context -> current,0,gcm_jump_cmd(info.jump_offset)); ++context -> current;
  rsxgl_draw_array_elements(context,rsx_primitive_type,count);
  gcm_emit_at(context -> current,0,gcm_return_cmd()); ++context -> current;

  return info.call_offset;
}

static inline uint32_t
rsxgl_draw_instances(gcmContextData * context,const uint32_t primcount,const uint32_t instanceid_index,const uint32_t call_offset)
{
  // TODO: Break instances into smaller batches based upon the size of the command buffer.

  const uint32_t call_cmd = gcm_call_cmd(call_offset);

  const uint32_t n = primcount * 4;
  uint32_t * buffer = gcm_reserve(context,n);
  
  for(uint32_t i = 0;i < primcount;++i,buffer += 4) {
    ieee32_t tmp;
    tmp.f = (float)i;
    
    gcm_emit_method_at(buffer,0,NV30_3D_VP_UPLOAD_CONST_ID,2);
    gcm_emit_at(buffer,1,instanceid_index);
    gcm_emit_at(buffer,2,tmp.u);
    gcm_emit_at(buffer,3,call_cmd);
  }

  gcm_finish_n_commands(context,n);
}

GLAPI void APIENTRY
glDrawArraysInstanced (GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
  rsxgl_context_t * ctx = current_ctx();
  gcmContextData * context = ctx -> gcm_context();

  RSXGL_FORWARD_ERROR_BEGIN();
  const uint32_t rsx_primitive_type = rsxgl_check_draw_arrays(ctx,mode);
  RSXGL_FORWARD_ERROR_END();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    const uint32_t timestamp = rsxgl_draw_init(ctx,mode,0,0,primcount);
    
    const rsxgl_query_object_index_type query_index = ctx -> any_samples_passed_query;
    
    const uint32_t instanceid_index = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].instanceid_index;
    if(instanceid_index != ~0 && primcount > 1) {
      rsxgl_draw_instances(context,primcount,instanceid_index,rsxgl_draw_arrays_instanced(context,rsx_primitive_type,first,count));
    }
    else {
      rsxgl_draw_arrays_instanced(context,rsx_primitive_type,first,count);
    }
    
    rsxgl_timestamp_post(ctx,timestamp + primcount - 1);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDrawElementsInstanced (const GLenum mode, const GLsizei count, const GLenum type, const GLvoid *indices, const GLsizei primcount)
{
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_END();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }
  
  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    gcmContextData * context = ctx -> gcm_context();
    
    const rsxgl_query_object_index_type query_index = ctx -> any_samples_passed_query;
    const uint32_t instanceid_index = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].instanceid_index;

    const uint32_t timestamp = rsxgl_draw_init(ctx,mode,0,0,primcount);    
    const rsxgl_draw_elements_info_t info = rsxgl_draw_elements_init(ctx,0,0,count,rsx_type,indices,timestamp);
    if(instanceid_index != ~0 && primcount > 1) {
      rsxgl_draw_instances(context,primcount,instanceid_index,rsxgl_draw_array_elements_instanced(context,rsx_primitive_type,count));
    }
    else {
      rsxgl_draw_array_elements(context,rsx_primitive_type,count);
    }    
    rsxgl_timestamp_post(ctx,timestamp + primcount - 1);
    rsxgl_draw_elements_exit(ctx,info);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glDrawElementsInstancedBaseVertex (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex)
{
  rsxgl_context_t * ctx = current_ctx();

  RSXGL_FORWARD_ERROR_BEGIN();
  uint32_t rsx_primitive_type = 0, rsx_type = 0;
  std::tie(rsx_primitive_type,rsx_type) = rsxgl_check_draw_elements(ctx,mode,type);
  RSXGL_FORWARD_ERROR_END();

  if(count < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(primcount < 0) {
    RSXGL_ERROR_(GL_INVALID_VALUE);
  }

  if(ctx -> state.enable.conditional_render_status != RSXGL_CONDITIONAL_RENDER_ACTIVE_WAIT_FAIL) {
    gcmContextData * context = ctx -> gcm_context();

    const rsxgl_query_object_index_type query_index = ctx -> any_samples_passed_query;
    const uint32_t instanceid_index = ctx -> program_binding[RSXGL_ACTIVE_PROGRAM].instanceid_index;
    
    const uint32_t timestamp = rsxgl_draw_init(ctx,mode,0,0,primcount);
    const rsxgl_draw_elements_info_t info = rsxgl_draw_elements_init(ctx,0,0,count,rsx_type,indices,timestamp);
    rsxgl_draw_array_elements_base(context,basevertex);
    if(instanceid_index != ~0 && primcount > 1) {
      rsxgl_draw_instances(context,primcount,instanceid_index,rsxgl_draw_array_elements_instanced(context,rsx_primitive_type,count));
    }
    else {
      rsxgl_draw_array_elements(context,rsx_primitive_type,count);
    }
    rsxgl_draw_array_elements_base(context,0);
    rsxgl_timestamp_post(ctx,timestamp + primcount - 1);
    rsxgl_draw_elements_exit(ctx,info);
  }

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glPrimitiveRestartIndex (GLuint index)
{
  rsxgl_context_t * ctx = current_ctx();
  ctx -> state.primitiveRestartIndex = index;
  ctx -> state.invalid.parts.primitive_restart = 1;
}

GLAPI void APIENTRY
glBeginTransformFeedback (GLenum primitiveMode)
{
  const uint32_t rsx_primitive_type = rsxgl_draw_mode(primitiveMode);

  if(!(rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_POINTS ||
       rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_LINES ||
       rsx_primitive_type == NV30_3D_VERTEX_BEGIN_END_TRIANGLES)) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> state.enable.transform_feedback_mode != 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  ctx -> state.enable.transform_feedback_mode = rsx_primitive_type;

  RSXGL_NOERROR_();
}

GLAPI void APIENTRY
glEndTransformFeedback (void)
{
  rsxgl_context_t * ctx = current_ctx();

  if(ctx -> state.enable.transform_feedback_mode == 0) {
    RSXGL_ERROR_(GL_INVALID_OPERATION);
  }

  ctx -> state.enable.transform_feedback_mode = 0;

  RSXGL_NOERROR_();
}
