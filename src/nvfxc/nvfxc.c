#include <stdio.h>

#include "ralloc.h"

#include "main/mtypes.h"

#include "pipe/p_defines.h"
#include "state_tracker/st_program.h"
#include "state_tracker/st_glsl_to_tgsi.h"
#include "nvfx/nvfx_context.h"
#include "nvfx/nvfx_state.h"

extern struct nvfx_vertex_program*
nvfx_vertprog_translate(struct nvfx_context *nvfx, const struct pipe_shader_state* vps, struct tgsi_shader_info* info);

extern struct nvfx_fragment_program*
nvfx_fragprog_translate(struct nvfx_context *nvfx,
                        struct nvfx_pipe_fragment_program *pfp,
                        boolean emulate_sprite_flipping);

struct gl_shader *
_abetts_new_shader(struct gl_context *ctx, GLuint name, GLenum type)
{
   struct gl_shader *shader;

   (void) ctx;

   assert(type == GL_FRAGMENT_SHADER || type == GL_VERTEX_SHADER);
   shader = rzalloc(NULL, struct gl_shader);
   if (shader) {
      shader->Type = type;
      shader->Name = name;
      shader->RefCount = 1;
   }
   return shader;
}

/**
 * Called via ctx->Driver.NewProgram() to allocate a new vertex or
 * fragment program.
 */
struct gl_program *
_abetts_new_program(struct gl_context *ctx, GLenum target, GLuint id)
{
  switch (target) {
  case GL_VERTEX_PROGRAM_ARB: {
    struct st_vertex_program *prog = ST_CALLOC_STRUCT(st_vertex_program);
    return _mesa_init_vertex_program(ctx, &prog->Base, target, id);
  }

  case GL_FRAGMENT_PROGRAM_ARB:
  case GL_FRAGMENT_PROGRAM_NV: {
    struct st_fragment_program *prog = ST_CALLOC_STRUCT(st_fragment_program);
    return _mesa_init_fragment_program(ctx, &prog->Base, target, id);
  }

  case MESA_GEOMETRY_PROGRAM: {
    struct st_geometry_program *prog = ST_CALLOC_STRUCT(st_geometry_program);
    return _mesa_init_geometry_program(ctx, &prog->Base, target, id);
  }

  default:
    assert(0);
    return NULL;
  }
}

/**
 * Called via ctx->Driver.ProgramStringNotify()
 * Called when the program's text/code is changed.  We have to free
 * all shader variants and corresponding gallium shaders when this happens.
 */
GLboolean
_abetts_program_string_notify( struct gl_context *ctx,
                                           GLenum target,
                                           struct gl_program *prog )
{
   return GL_TRUE;
}

static unsigned
st_translate_interp(enum glsl_interp_qualifier glsl_qual, bool is_color)
{
  switch (glsl_qual) {
  case INTERP_QUALIFIER_NONE:
    if (is_color)
      return TGSI_INTERPOLATE_LINEAR;
    return TGSI_INTERPOLATE_PERSPECTIVE;
  case INTERP_QUALIFIER_SMOOTH:
    return TGSI_INTERPOLATE_PERSPECTIVE;
  case INTERP_QUALIFIER_FLAT:
    return TGSI_INTERPOLATE_CONSTANT;
  case INTERP_QUALIFIER_NOPERSPECTIVE:
    return TGSI_INTERPOLATE_LINEAR;
  default:
    assert(0 && "unexpected interp mode in st_translate_interp()");
    return TGSI_INTERPOLATE_PERSPECTIVE;
  }
}

void nvfxc(struct gl_context *ctx,struct gl_shader_program *whole_program)
{
  // 
  st_link_shader(ctx, whole_program);
  
  // gl_program's are in:
  // whole_program->_LinkedShaders[]->Program
  
  if(whole_program->_LinkedShaders[MESA_SHADER_VERTEX] != 0 && whole_program->_LinkedShaders[MESA_SHADER_VERTEX]->Program != 0) {
    fprintf(stderr,"processing VP\n");
    struct st_vertex_program * vp = st_vertex_program((struct gl_vertex_program *)whole_program->_LinkedShaders[MESA_SHADER_VERTEX]->Program);
    st_prepare_vertex_program(ctx, vp);
    
    fprintf(stderr," inputs: %u outputs: %u\n",
	    vp->num_inputs,vp->num_outputs);
    
    struct ureg_program * ureg = ureg_create(TGSI_PROCESSOR_VERTEX);
    
    if(ureg == 0) {
      fprintf(stderr," ureg_create failed\n");
      return;
    }
    
    enum pipe_error error = st_translate_program(ctx,
						 TGSI_PROCESSOR_VERTEX,
						 ureg,
						 vp->glsl_to_tgsi,
						 &vp->Base.Base,
						 /* inputs */
						 vp->num_inputs,
						 vp->input_to_index,
						 NULL, /* input semantic name */
						 NULL, /* input semantic index */
						 NULL, /* interp mode */
						 /* outputs */
						 vp->num_outputs,
						 vp->result_to_output,
						 vp->output_semantic_name,
						 vp->output_semantic_index,
						 0 );
    if(error) {
      fprintf(stderr," st_translate_program failed\n");
    }
    else {
      struct pipe_shader_state tgsi;
      tgsi.tokens = ureg_get_tokens(ureg,NULL);
      if(tgsi.tokens == 0) {
	fprintf(stderr," ureg_get_tokens failed\n");
	return;
      }
      else {
	st_translate_stream_output_info(vp->glsl_to_tgsi,
					vp->result_to_output,
					&tgsi.stream_output);

	//
	struct tgsi_shader_info info;
	tgsi_scan_shader(tgsi.tokens,&info);
	
	// now call nvfx stuff:
	struct nvfx_context nvfx;
	nvfx.is_nv4x = ~0;
	nvfx.use_nv4x = ~0;
	nvfx.use_vp_clipping = TRUE;

	struct nvfx_vertex_program * nvfx_vp = nvfx_vertprog_translate(&nvfx,&tgsi,&info);
      }
    }
    
    ureg_destroy(ureg);
  }

  if(whole_program->_LinkedShaders[MESA_SHADER_FRAGMENT] != 0 && whole_program->_LinkedShaders[MESA_SHADER_FRAGMENT]->Program != 0) {
    fprintf(stderr,"processing FP\n");
    struct st_fragment_program * stfp = st_fragment_program((struct gl_fragment_program *)whole_program->_LinkedShaders[MESA_SHADER_FRAGMENT]->Program);

    if (!stfp->tgsi.tokens) {
      /* need to translate Mesa instructions to TGSI now */
      GLuint outputMapping[FRAG_RESULT_MAX];
      GLuint inputMapping[FRAG_ATTRIB_MAX];
      GLuint interpMode[PIPE_MAX_SHADER_INPUTS];  /* XXX size? */
      GLuint attr;
      const GLbitfield64 inputsRead = stfp->Base.Base.InputsRead;
      struct ureg_program *ureg;

      GLboolean write_all = GL_FALSE;

      ubyte input_semantic_name[PIPE_MAX_SHADER_INPUTS];
      ubyte input_semantic_index[PIPE_MAX_SHADER_INPUTS];
      uint fs_num_inputs = 0;

      ubyte fs_output_semantic_name[PIPE_MAX_SHADER_OUTPUTS];
      ubyte fs_output_semantic_index[PIPE_MAX_SHADER_OUTPUTS];
      uint fs_num_outputs = 0;
      
      if (!stfp->glsl_to_tgsi)
         _mesa_remove_output_reads(&stfp->Base.Base, PROGRAM_OUTPUT);

      /*
       * Convert Mesa program inputs to TGSI input register semantics.
       */
      for (attr = 0; attr < FRAG_ATTRIB_MAX; attr++) {
         if ((inputsRead & BITFIELD64_BIT(attr)) != 0) {
            const GLuint slot = fs_num_inputs++;

            inputMapping[attr] = slot;

            switch (attr) {
            case FRAG_ATTRIB_WPOS:
               input_semantic_name[slot] = TGSI_SEMANTIC_POSITION;
               input_semantic_index[slot] = 0;
               interpMode[slot] = TGSI_INTERPOLATE_LINEAR;
               break;
            case FRAG_ATTRIB_COL0:
               input_semantic_name[slot] = TGSI_SEMANTIC_COLOR;
               input_semantic_index[slot] = 0;
               interpMode[slot] = st_translate_interp(stfp->Base.InterpQualifier[attr],
                                                      TRUE);
               break;
            case FRAG_ATTRIB_COL1:
               input_semantic_name[slot] = TGSI_SEMANTIC_COLOR;
               input_semantic_index[slot] = 1;
               interpMode[slot] = st_translate_interp(stfp->Base.InterpQualifier[attr],
                                                      TRUE);
               break;
            case FRAG_ATTRIB_FOGC:
               input_semantic_name[slot] = TGSI_SEMANTIC_FOG;
               input_semantic_index[slot] = 0;
               interpMode[slot] = TGSI_INTERPOLATE_PERSPECTIVE;
               break;
            case FRAG_ATTRIB_FACE:
               input_semantic_name[slot] = TGSI_SEMANTIC_FACE;
               input_semantic_index[slot] = 0;
               interpMode[slot] = TGSI_INTERPOLATE_CONSTANT;
               break;
            case FRAG_ATTRIB_CLIP_DIST0:
               input_semantic_name[slot] = TGSI_SEMANTIC_CLIPDIST;
               input_semantic_index[slot] = 0;
               interpMode[slot] = TGSI_INTERPOLATE_LINEAR;
               break;
            case FRAG_ATTRIB_CLIP_DIST1:
               input_semantic_name[slot] = TGSI_SEMANTIC_CLIPDIST;
               input_semantic_index[slot] = 1;
               interpMode[slot] = TGSI_INTERPOLATE_LINEAR;
               break;
               /* In most cases, there is nothing special about these
                * inputs, so adopt a convention to use the generic
                * semantic name and the mesa FRAG_ATTRIB_ number as the
                * index. 
                * 
                * All that is required is that the vertex shader labels
                * its own outputs similarly, and that the vertex shader
                * generates at least every output required by the
                * fragment shader plus fixed-function hardware (such as
                * BFC).
                * 
                * There is no requirement that semantic indexes start at
                * zero or be restricted to a particular range -- nobody
                * should be building tables based on semantic index.
                */
            case FRAG_ATTRIB_PNTC:
            case FRAG_ATTRIB_TEX0:
            case FRAG_ATTRIB_TEX1:
            case FRAG_ATTRIB_TEX2:
            case FRAG_ATTRIB_TEX3:
            case FRAG_ATTRIB_TEX4:
            case FRAG_ATTRIB_TEX5:
            case FRAG_ATTRIB_TEX6:
            case FRAG_ATTRIB_TEX7:
            case FRAG_ATTRIB_VAR0:
            default:
               /* Actually, let's try and zero-base this just for
                * readability of the generated TGSI.
                */
               assert(attr >= FRAG_ATTRIB_TEX0);
               input_semantic_index[slot] = (attr - FRAG_ATTRIB_TEX0);
               input_semantic_name[slot] = TGSI_SEMANTIC_GENERIC;
               if (attr == FRAG_ATTRIB_PNTC)
                  interpMode[slot] = TGSI_INTERPOLATE_LINEAR;
               else
                  interpMode[slot] = st_translate_interp(stfp->Base.InterpQualifier[attr],
                                                         FALSE);
               break;
            }
         }
         else {
            inputMapping[attr] = -1;
         }
      }

      /*
       * Semantics and mapping for outputs
       */
      {
         uint numColors = 0;
         GLbitfield64 outputsWritten = stfp->Base.Base.OutputsWritten;

         /* if z is written, emit that first */
         if (outputsWritten & BITFIELD64_BIT(FRAG_RESULT_DEPTH)) {
            fs_output_semantic_name[fs_num_outputs] = TGSI_SEMANTIC_POSITION;
            fs_output_semantic_index[fs_num_outputs] = 0;
            outputMapping[FRAG_RESULT_DEPTH] = fs_num_outputs;
            fs_num_outputs++;
            outputsWritten &= ~(1 << FRAG_RESULT_DEPTH);
         }

         if (outputsWritten & BITFIELD64_BIT(FRAG_RESULT_STENCIL)) {
            fs_output_semantic_name[fs_num_outputs] = TGSI_SEMANTIC_STENCIL;
            fs_output_semantic_index[fs_num_outputs] = 0;
            outputMapping[FRAG_RESULT_STENCIL] = fs_num_outputs;
            fs_num_outputs++;
            outputsWritten &= ~(1 << FRAG_RESULT_STENCIL);
         }

         /* handle remaning outputs (color) */
         for (attr = 0; attr < FRAG_RESULT_MAX; attr++) {
            if (outputsWritten & BITFIELD64_BIT(attr)) {
               switch (attr) {
               case FRAG_RESULT_DEPTH:
               case FRAG_RESULT_STENCIL:
                  /* handled above */
                  assert(0);
                  break;
               case FRAG_RESULT_COLOR:
                  write_all = GL_TRUE; /* fallthrough */
               default:
                  assert(attr == FRAG_RESULT_COLOR ||
                         (FRAG_RESULT_DATA0 <= attr && attr < FRAG_RESULT_MAX));
                  fs_output_semantic_name[fs_num_outputs] = TGSI_SEMANTIC_COLOR;
                  fs_output_semantic_index[fs_num_outputs] = numColors;
                  outputMapping[attr] = fs_num_outputs;
                  numColors++;
                  break;
               }

               fs_num_outputs++;
            }
         }
      }

      ureg = ureg_create( TGSI_PROCESSOR_FRAGMENT );

      if(ureg == 0) {
	fprintf(stderr," ureg_create failed\n");
	return;
      }

      if (write_all == GL_TRUE)
         ureg_property_fs_color0_writes_all_cbufs(ureg, 1);

      if (stfp->Base.FragDepthLayout != FRAG_DEPTH_LAYOUT_NONE) {
         switch (stfp->Base.FragDepthLayout) {
         case FRAG_DEPTH_LAYOUT_ANY:
            ureg_property_fs_depth_layout(ureg, TGSI_FS_DEPTH_LAYOUT_ANY);
            break;
         case FRAG_DEPTH_LAYOUT_GREATER:
            ureg_property_fs_depth_layout(ureg, TGSI_FS_DEPTH_LAYOUT_GREATER);
            break;
         case FRAG_DEPTH_LAYOUT_LESS:
            ureg_property_fs_depth_layout(ureg, TGSI_FS_DEPTH_LAYOUT_LESS);
            break;
         case FRAG_DEPTH_LAYOUT_UNCHANGED:
            ureg_property_fs_depth_layout(ureg, TGSI_FS_DEPTH_LAYOUT_UNCHANGED);
            break;
         default:
            assert(0);
         }
      }

      st_translate_program(ctx,
                              TGSI_PROCESSOR_FRAGMENT,
                              ureg,
                              stfp->glsl_to_tgsi,
                              &stfp->Base.Base,
                              /* inputs */
                              fs_num_inputs,
                              inputMapping,
                              input_semantic_name,
                              input_semantic_index,
                              interpMode,
                              /* outputs */
                              fs_num_outputs,
                              outputMapping,
                              fs_output_semantic_name,
                              fs_output_semantic_index, FALSE );

      stfp->tgsi.tokens = ureg_get_tokens( ureg, NULL );

      if(stfp->tgsi.tokens == 0) {
	fprintf(stderr," ureg_get_tokens failed\n");
	return;
      }
      else {
	struct nvfx_pipe_fragment_program fp;

	fp.pipe.tokens = stfp->tgsi.tokens;
	tgsi_scan_shader(stfp->tgsi.tokens,&fp.info);

	// now call nvfx stuff:
	struct nvfx_context nvfx;
	nvfx.is_nv4x = ~0;
	nvfx.use_nv4x = ~0;
	nvfx.use_vp_clipping = TRUE;

	struct nvfx_fragment_program * nvfx_fp = nvfx_fragprog_translate(&nvfx,&fp,FALSE);
      }

      ureg_destroy( ureg );
    }
  }
}
