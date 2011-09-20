//-*-C-*-

#ifndef nv40prog_H
#define nv40prog_H
#define __RSX_PROGRAM_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

typedef union {
  float f;
  uint32_t u;
  struct {
    uint16_t a[2];
  } h;
  struct {
    uint8_t a[4];
  } b;
} ieee32_t;

enum rsx_param_types {
  PARAM_FLOAT = 0,
  PARAM_FLOAT2 = 1,
  PARAM_FLOAT3 = 2,
  PARAM_FLOAT4 = 3,
  PARAM_FLOAT4x4 = 4,
  PARAM_SAMPLER1D = 5,
  PARAM_SAMPLER2D = 6,
  PARAM_SAMPLER3D = 7,
  PARAM_SAMPLERCUBE = 8,
  PARAM_SAMPLERRECT = 9,
  PARAM_UNKNOWN = 0xff
};

/*! \brief Vertex program data structure.

This data structure is filled by cgcomp, the offline compiler for shader programs. */
typedef struct rsx_vp
{
  uint16_t magic;			/*!< \brief magic identifier */
  uint16_t num_attrib;		/*!< \brief number of used input attributes in the vertex program */
  uint32_t attrib_off;		/*!< \brief offset to the attribute name table */
  
  uint32_t input_mask;		/*!< \brief mask of input attributes in the vertex shader */
  uint32_t output_mask;	/*!< \brief mask of result attributes passed to the fragment shader */
  
  uint16_t const_start;	/*!< \brief start address in vertex shader's constant block memory */
  uint16_t num_const;		/*!< \brief number of used constants in the vertex program */
  uint32_t const_off;		/*!< \brief offset to the constant name table */

  uint16_t start_insn;		/*!< \brief start address to load the vertex program to */
  uint16_t num_insn;		/*!< \brief number of vertex shader instructions */
  uint32_t ucode_off;		/*!< \brief offset to the shader's micro code */
} rsxVertexProgram;
  
/*! \brief Fragment program data structure.
    
  This data structure is filled by cgcomp, the offline compiler for shader programs. */
typedef struct rsx_fp
{
  uint16_t magic;			/*!< \brief magic identifier */ 
  uint16_t num_attrib;		/*!< \brief number of used input attributes in the fragment program */
  uint32_t attrib_off;		/*!< \brief offset to the attribute name table */
  
  uint32_t num_regs;		/*!< \brief number of used registers in the fragment program */
  uint32_t fp_control;		/*!< \brief fragment program control mask */
  
  uint16_t texcoords;		/*!< \brief bit mask of all used texture coords in the fragment program */
  uint16_t texcoord2D;		/*!< \brief bit mask of used 2D texture coords in the fragment program */
  uint16_t texcoord3D;		/*!< \brief bit mask of used 3D texture coords in the fragment program */
  
  uint16_t _pad0;			/*!< \brief unused padding word. most be 0 */
  
  uint16_t num_const;		/*!< \brief number of used constants in the fragment program */
  uint32_t const_off;		/*!< \brief offset to the constant name table */
  
  uint16_t num_insn;		/*!< \brief number of fragment program instructions */
  uint32_t ucode_off;		/*!< \brief offset to the shaders's micro code */
} rsxFragmentProgram;

/*! \brief Program const data structure. */
typedef struct rsx_const
{
  uint32_t name_off;		/*!< \brief offset of name. */
  uint32_t index;			/*!< \brief program const id. */
  uint8_t type;			/*!< \brief program const type. */
  uint8_t is_internal;		/*!< \brief internal flag. */
  uint8_t count;			/*!< \brief number of elements in the const. */
  
  uint8_t _pad0;			/*!< \brief unused padding byte, most be 0. */
  
  ieee32_t values[4];

} rsxProgramConst;

/*! \brief Table of const offsets. */
typedef struct rsx_co_table
{
  uint32_t num;		/*!< \brief number of elements in the array. */
  uint32_t offset[];	/*!< \brief array of const offsets. */
} rsxConstOffsetTable;

/*! \brief Table of program attributes. */
typedef struct rsx_attrib
{
  uint32_t name_off;	/*!< \brief offset of attribute name. */
  uint32_t index;		/*!< \brief attribute index. */
  uint8_t type;		/*!< \brief attribute type. */
  uint8_t is_output;       /*!< \brief Is it an output or an input? */
  uint8_t _pad0[2];
} rsxProgramAttrib;

/*! \brief Get Ucode from RSX vertex program.
\param vp Pointer the to vertex program structure.
\return Pointer to the ucode.
*/
void* rsxVertexProgramGetUCode(rsxVertexProgram *vp);

/*! \brief Get the list of vertex program consts.
\param vp Pointer the to vertex program structure.
\return Pointer to the list of program const structures.
*/
rsxProgramConst* rsxVertexProgramGetConsts(rsxVertexProgram *vp);

/*! \brief Get id of vertex program const from its name.
\param vp Pointer the to vertex program structure.
\param name Name of the vertex program const.
\return The requested vertex program const id.
*/
int32_t rsxVertexProgramGetConst(rsxVertexProgram *vp,const char *name);

/*! \brief Get the list of vertex program attributes.
\param vp Pointer the to vertex program structure.
\return Pointer to the list of program attribute structures.
*/
rsxProgramAttrib* rsxVertexProgramGetAttribs(rsxVertexProgram *vp);

/*! \brief Get id of vertex program attribute from its name.
\param vp Pointer the to vertex program structure.
\param name Name of the vertex program attribute.
\return The requested vertex program attribute id.
*/
int32_t rsxVertexProgramGetAttrib(rsxVertexProgram *vp,const char *name);

/*! \brief Get Ucode from RSX fragment program.
\param fp Pointer the to fragment program structure.
\return Pointer to the ucode.
*/
void* rsxFragmentProgramGetUCode(rsxFragmentProgram *fp,uint32_t *size);

/*! \brief Get the list of fragment program consts.
\param fp Pointer the to fragment program structure.
\return Pointer to the list of program const structures.
*/
rsxProgramConst* rsxFragmentProgramGetConsts(rsxFragmentProgram *fp);

/*! \brief Get id of fragment program const from its name.
\param fp Pointer the to fragment program structure.
\param name Name of the fragment program const.
\return The requested fragment program const id.
*/
int32_t rsxFragmentProgramGetConst(rsxFragmentProgram *fp,const char *name);

/*! \brief Get the list of fragment program attributes.
\param fp Pointer the to fragment program structure.
\return Pointer to the list of program attribute structures.
*/
rsxProgramAttrib* rsxFragmentProgramGetAttribs(rsxFragmentProgram *fp);

/*! \brief Get id of fragment program attribute from its name.
\param fp Pointer the to fragment program structure.
\param name Name of the fragment program attribute.
\return The requested fragment program attribute id.
*/
int32_t rsxFragmentProgramGetAttrib(rsxFragmentProgram *fp,const char *name);

/*! \brief Get const offset table from a fragment program.
\param fp Pointer the to fragment program structure.
\param table_off Offset of the const offset table.
\return Pointer to the requested const offset table.
*/
rsxConstOffsetTable* rsxFragmentProgramGetConstOffsetTable(rsxFragmentProgram *fp,uint32_t table_off);

#if defined(__cplusplus)
}
#endif

#endif
