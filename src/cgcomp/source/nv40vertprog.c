#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nv40prog.h>

void *
rsxVertexProgramGetUCode(rsxVertexProgram *vp)
{
  return (void*)(((uint8_t*)vp) + vp->ucode_off);
}

rsxProgramConst *
rsxVertexProgramGetConsts(rsxVertexProgram *vp)
{
  return (rsxProgramConst*)(((uint8_t*)vp) + vp->const_off);
}

int32_t
rsxVertexProgramGetConst(rsxVertexProgram *vp,const char *name)
{
  uint32_t i;
  rsxProgramConst *vpc = rsxVertexProgramGetConsts(vp);
  
  for(i=0;i<vp->num_const;i++) {
    char *namePtr;
    
    if(!vpc[i].name_off) continue;
    
    namePtr = ((char*)vp) + vpc[i].name_off;
    if(strcasecmp(name,namePtr)==0)
      return i;
  }
  return -1;
}

rsxProgramAttrib *
rsxVertexProgramGetAttribs(rsxVertexProgram *vp)
{
  return (rsxProgramAttrib*)(((uint8_t*)vp) + vp->attrib_off);
}

int32_t
rsxVertexProgramGetAttrib(rsxVertexProgram *vp,const char *name)
{
  uint32_t i;
  rsxProgramAttrib *attribs = rsxVertexProgramGetAttribs(vp);
  
  for(i=0;i<vp->num_attrib;i++) {
    char *namePtr;
    
    if(!attribs[i].name_off) continue;
    
    namePtr = ((char*)vp) + attribs[i].name_off;
    if(strcasecmp(name,namePtr)==0)
      return attribs[i].index;
  }
  return -1;
}
