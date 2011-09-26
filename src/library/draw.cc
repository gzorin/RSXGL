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
template< uint32_t max_method_args, typename Operations >
void rsxgl_process_batch(gcmContextData * context,const uint32_t n,const Operations & operations)
{
  //static const uint32_t batch_size = Operations::batch_size;
  //static const uint32_t batch_size_bits = boost::static_log2< batch_size >::value;
  //const uint32_t nbatch = n >> batch_size_bits;
  //const uint32_t nbatchremainder = n & (batch_size - 1);

  const boost::tuple< uint32_t, uint32_t, uint32_t > tmp = operations.work_info();
  const uint32_t batch_size = tmp.get<0>();
  const uint32_t nbatch = tmp.get<1>();
  const uint32_t nbatchremainder = tmp.get<2>();

  uint32_t ninvoc = nbatch / max_method_args;
  const uint32_t ninvocremainder = nbatch % max_method_args;

  operations.begin(context,ninvoc,ninvocremainder,nbatchremainder);

  for(;ninvoc > 0;--ninvoc) {
    operations.begin_group(max_method_args);
    for(size_t i = 0;i < max_method_args;++i) {
      operations.begin_batch(i,batch_size);
    }
    operations.end_group(max_method_args);
  }

  // last group and last batch both together:
  if((ninvocremainder > 0) && (nbatchremainder > 0) && (ninvocremainder < max_method_args)) {
    operations.begin_group(ninvocremainder + 1);
    for(size_t i = 0;i < ninvocremainder;++i) {
      operations.begin_batch(i,batch_size);
    }
    operations.begin_batch(ninvocremainder,nbatchremainder);
    operations.end_group(ninvocremainder + 1);
  }
  else {
    // last group
    if(ninvocremainder > 0) {
      operations.begin_group(ninvocremainder);
      for(size_t i = 0;i < ninvocremainder;++i) {
	operations.begin_batch(i,batch_size);
      }
      operations.end_group(ninvocremainder);
    }
    
    // last batch
    if(nbatchremainder > 0) {
      operations.begin_group(1);
      operations.begin_batch(0,nbatchremainder);
      operations.end_group(1);
    }
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

// Operations performed by rsxgl_process_batch (a local class passed as a template argument is a C++0x feature):
struct rsxgl_draw_array_operations {
  static const uint32_t batch_size = RSXGL_MAX_DRAW_BATCH_SIZE;
  static const uint32_t batch_size_bits = boost::static_log2< batch_size >::value;
  
  mutable uint32_t * buffer, * buffer_begin;
  mutable uint32_t current, groupcurrent;
  const uint32_t count, rsx_primitive_type;
  
  rsxgl_draw_array_operations(const uint32_t _rsx_primitive_type,const uint32_t first,const uint32_t _count)
    : buffer(0), buffer_begin(0), current(first), groupcurrent(0), count((_count / 3) * 3), rsx_primitive_type(_rsx_primitive_type) {
  }

  // returns batch size, number of batches, plus remainder vertices:
  boost::tuple< uint32_t, uint32_t, uint32_t > work_info() const {
    static const uint32_t triangle_batch_size = (batch_size / 3) * 3;
    return boost::make_tuple(triangle_batch_size,count / triangle_batch_size,count % triangle_batch_size);
  }
  
  // ninvoc - number of full draw method invocations (2047 * 256 vertices)
  // ninvocremainder - number of vertex batches for an additional draw method invocation (ninvocremainder * 256 vertices)
  // nbatchremainder - size of one additional vertex batch (nbatchremainder vertices)
  inline void
  begin(gcmContextData * context,const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) const {
    // count the total number of words that will be added to the command stream:
    const bool combine_invocremainder_and_batchremainder = (ninvocremainder > 0) && (ninvocremainder < RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS);
    const uint32_t nmethods = ninvoc + (ninvocremainder ? 1 : 0) + ((nbatchremainder && !combine_invocremainder_and_batchremainder) ? 1 : 0);
    const uint32_t nargs = (ninvoc * RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS) + ninvocremainder + (nbatchremainder ? 1 : 0);
    const uint32_t nwords = (nmethods * (5 + 8)) + nargs;
    
    buffer = gcm_reserve(context,nwords);
    buffer_begin = buffer;
  }
  
  // n is number of arguments to this method:
  inline void
  begin_group(const uint32_t n) const {
    // perform rsxgl_draw_begin
    // how often is it necessary to perform this cache invalidation? here it's done for every batch of 256 vertices
    gcm_emit_method_at(buffer,0,NV40_3D_VTX_CACHE_INVALIDATE,1);
    gcm_emit_at(buffer,1,0);
    
    gcm_emit_method_at(buffer,2,NV40_3D_VTX_CACHE_INVALIDATE,1);
    gcm_emit_at(buffer,3,0);
    
    gcm_emit_method_at(buffer,4,NV40_3D_VTX_CACHE_INVALIDATE,1);
    gcm_emit_at(buffer,5,0);
    
    gcm_emit_method_at(buffer,6,NV30_3D_VERTEX_BEGIN_END,1);
    gcm_emit_at(buffer,7,rsx_primitive_type);
    gcm_emit_method_at(buffer,8,NV30_3D_VB_VERTEX_BATCH,n);
    
    buffer += 9;
    
    groupcurrent = 0;
  }
  
  // n is the size of this batch (the number of vertices in this batch):
  inline void
  begin_batch(const uint32_t igroup,const uint32_t n) const {
    // for triangles only:
    const uint32_t actual_n = n /*- (n % 3)*/ ;
    
    gcm_emit_at(buffer,igroup,((actual_n - 1) << NV30_3D_VB_VERTEX_BATCH_COUNT__SHIFT) | current);
    
    current += actual_n;
    groupcurrent += actual_n;
  }
  
  inline void
  end_group(const uint32_t n) const {
    buffer += n;
    
    gcm_emit_method_at(buffer,0,NV30_3D_VERTEX_BEGIN_END,1);
    gcm_emit_at(buffer,1,NV30_3D_VERTEX_BEGIN_END_STOP);
    
    buffer += 2;
  }
  
  inline void
  end() const {
  }
};

static inline void
rsxgl_draw_arrays(gcmContextData * context,const uint32_t rsx_primitive_type,const uint32_t first,const uint32_t count)
{
  rsxgl_draw_array_operations op(rsx_primitive_type,first,count);
  rsxgl_process_batch< RSXGL_VERTEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);

  // this is the equivalent of gcm_finish_commands():
  context -> current = op.buffer;
}

#define RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS 1

// Operations performed by rsxgl_process_batch (a local class passed as a template argument is a C++0x feature):
struct rsxgl_draw_array_elements_operations {
  static const uint32_t batch_size = RSXGL_MAX_DRAW_BATCH_SIZE;
  static const uint32_t batch_size_bits = boost::static_log2< batch_size >::value;
  
  mutable uint32_t * buffer;
  mutable uint32_t current;
  const uint32_t count, rsx_primitive_type;
  
  rsxgl_draw_array_elements_operations(const uint32_t _rsx_primitive_type,const uint32_t _count)
    : buffer(0), current(0), count(_count), rsx_primitive_type(_rsx_primitive_type) {
  }
  
  inline void
  begin(gcmContextData * context,const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) const {
    // count the total number of words that will be added to the command stream:
    const bool combine_invocremainder_and_batchremainder = (ninvocremainder > 0) && (ninvocremainder < RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS);
    const uint32_t nmethods = ninvoc + (ninvocremainder ? 1 : 0) + ((nbatchremainder && !combine_invocremainder_and_batchremainder) ? 1 : 0);
    const uint32_t nargs = (ninvoc * RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS) + ninvocremainder + (nbatchremainder ? 1 : 0);
    const uint32_t nwords = (nmethods * (5)) + nargs;
    
    buffer = gcm_reserve(context,nwords);
  }
  
  // n is number of arguments to this method:
  inline void
  begin_group(const uint32_t n) const {
    // Interestingly, draw_array_elements does /not/ require invalidating the vertex cache, even if indices stored in the
    // elements performs the equivalent to glDrawArrays() (i.e., the indices increase, monotonically, by a value of 1).
    // TODO - Make compile time option to turn glDrawArrays() into a specialization of glDrawArrays()???
    
    gcm_emit_method_at(buffer,0,NV30_3D_VERTEX_BEGIN_END,1);
    gcm_emit_at(buffer,1,rsx_primitive_type);
    gcm_emit_method_at(buffer,2,NV30_3D_VB_INDEX_BATCH,n);
    
    buffer += 3;
  }
  
  // n is the size of this batch:
  inline void
  begin_batch(const uint32_t igroup,const uint32_t n) const {
    gcm_emit_at(buffer,igroup,((n - 1) << NV30_3D_VB_INDEX_BATCH_COUNT__SHIFT) | current);
    current += n;
  }
  
  inline void
  end_group(const uint32_t n) const {
    buffer += n;
    
    gcm_emit_method_at(buffer,0,NV30_3D_VERTEX_BEGIN_END,1);
    gcm_emit_at(buffer,1,NV30_3D_VERTEX_BEGIN_END_STOP);
    
    buffer += 2;
  }
  
  inline void
  end() const {
  }
};

static inline void
rsxgl_draw_array_elements(gcmContextData * context,const uint32_t rsx_primitive_type,const uint32_t count)
{
#if 0
  rsxgl_draw_array_elements_operations op(rsx_primitive_type,count);
  rsxgl_process_batch< RSXGL_INDEX_BATCH_MAX_FIFO_METHOD_ARGS > (context,count,op);
  context -> current = op.buffer;
#endif
}

#if 0
// This doesn't work:
template< GLenum ElementType >
static inline void
rsxgl_draw_inline_elements(gcmContextData * context,uint32_t count,uint32_t const * elements)
{
  // Operations performed by rsxgl_process_batch (a local class passed as a template argument is a C++0x feature):
  struct operations {
    mutable uint32_t const * elements;
    mutable uint32_t * buffer;
    mutable uint32_t current, lastgroupn;

    operations(uint32_t const * _elements)
      : elements(_elements), buffer(0), current(0), lastgroupn(0) {
    }

    inline void
    begin(gcmContextData * context,const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) const {
      const uint32_t n =
	// method + 2047 arguments:
	((RSXGL_MAX_FIFO_METHOD_ARGS + 1) * ninvoc) +
	// method + ninvocremainder arguments:
	((ninvocremainder > 0) ? (1 + ninvocremainder) : 0) +
	// method + 1 argument:
	((nbatchremainder > 0) ? 2 : 0);

      buffer = gcm_reserve(context,n);
      lastgroupn = 0;
    }

    // n is number of arguments to this method:
    inline void
    begin_group(const uint32_t n) const {
      //buffer += lastgroupn;
      lastgroupn = n;

      gcm_emit_method_at(buffer,0,NV30_3D_VB_ELEMENT_U32,n);
      ++buffer;
    }

    // n is the size of this batch (1, in this case):
    inline void
    begin_batch(const uint32_t igroup,const uint32_t) const {
      //gcm_emit_at(buffer,igroup,elements[current]);
      //gcm_emit_at(buffer,igroup,0);
      gcm_emit_at(buffer,0,*elements);
      ++elements;
      ++buffer;
      ++current;
    }

    inline void
    end() const {
      //buffer += lastgroupn;
    }
  };

  operations op(elements);
  rsxgl_process_batch< 1, RSXGL_MAX_FIFO_METHOD_ARGS > (context,count,op);
  context -> current = op.buffer;
}
#endif

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

#if 0
  // This (passing indices over the FIFO) doesn't work:
  // TODO - for index array over some threshold, migrate to GPU memory instead of transferring over the FIFO.
  if(client_indices) {
    uint32_t const * _indices = (uint32_t const *)indices;

    if(type == GL_UNSIGNED_INT) {
      rsxgl_draw_inline_elements< GL_UNSIGNED_INT >(context,count,_indices);
    }
    else if(type == GL_UNSIGNED_SHORT) {
    }
  }
  else {
    rsxgl_draw_array_elements(context,count);
  }
#endif

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
