#include <stdint.h>
#include <string.h>
#include <nv40prog.h>

void *
rsxFragmentProgramGetUCode(rsxFragmentProgram *fp,uint32_t *size)
{
  *size = fp->num_insn*sizeof(uint32_t)*4;
  return (void*)(((uint8_t*)fp) + fp->ucode_off);
}

rsxProgramConst *
rsxFragmentProgramGetConsts(rsxFragmentProgram *fp)
{
  return (rsxProgramConst*)(((uint8_t*)fp) + fp->const_off);
}

int32_t
rsxFragmentProgramGetConst(rsxFragmentProgram *fp,const char *name)
{
  uint32_t i;
  rsxProgramConst *fpc = rsxFragmentProgramGetConsts(fp);
  
  for(i=0;i<fp->num_const;i++) {
    char *namePtr;
    
    if(!fpc[i].name_off) continue;
    
    namePtr = ((char*)fp) + fpc[i].name_off;
    if(strcasecmp(name,namePtr)==0)
      return i;
  }
  return -1;
}

rsxProgramAttrib *
rsxFragmentProgramGetAttribs(rsxFragmentProgram *fp)
{
  return (rsxProgramAttrib*)(((uint8_t*)fp) + fp->attrib_off);
}

int32_t
rsxFragmentProgramGetAttrib(rsxFragmentProgram *fp,const char *name)
{
  uint32_t i;
  rsxProgramAttrib *attribs = rsxFragmentProgramGetAttribs(fp);
  
  for(i=0;i<fp->num_attrib;i++) {
    char *namePtr;
    
    if(!attribs[i].name_off) continue;
    
    namePtr = ((char*)fp) + attribs[i].name_off;
    if(strcasecmp(name,namePtr)==0)
      return attribs[i].index;
  }
  return -1;
}

rsxConstOffsetTable*
rsxFragmentProgramGetConstOffsetTable(rsxFragmentProgram *fp,uint32_t table_off)
{
  return (rsxConstOffsetTable*)(((uint8_t*)fp) + table_off);
}
