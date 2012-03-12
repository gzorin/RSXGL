#include <unistd.h>
#include <fstream>
#include <iostream>
#include <string>

#include "types.h"
#include "fpparser.h"
#include "vpparser.h"
#include "compiler.h"
#include "compilerfp.h"

#if !defined(WIN32)
#include <dlfcn.h>
#endif

#define PROG_TYPE_NONE			0
#define PROG_TYPE_VP			1
#define PROG_TYPE_FP			2

#ifdef __BIG_ENDIAN__
#define SWAP16(v) (v)
#define SWAP32(v) (v)
#else
#define SWAP16(v) ((v)>>8)|((v)<<8)
#define SWAP32(v) ((v)>>24)|((v)<<24)|(((v)&0xFF00)<<8)|(((v)&0xFF0000)>>8)
#endif


static u32 endian_fp(u32 v)
{
  return ( ( ( v >> 16 ) & 0xffff ) << 0 ) |
         ( ( ( v >> 0 ) & 0xffff ) << 16 );
}

void usage()
{
  std::cerr << "Usage: nv40asm [options] [input]\n" << std::endl;
  std::cerr << "Options\n" << std::endl;
  std::cerr << "\t-f\t\tInput is fragment program\n" << std::endl;
  std::cerr << "\t-v\t\tInput is vertex program\n" << std::endl;
  std::cerr << "\t-o <filename>\tWrite output to <filename> instead of to stdout\n" << std::endl;
}

std::string
readinput(std::istream & in)
{
  std::string result;
  int first = 1;

  while(!in.eof() && in.good()) {
    std::string tmp;
    std::getline(in,tmp);

    if(first) {
      result += tmp;
      first = 0;
    }
    else {
      result += "\n" + tmp;
    }
  }

  if(!in.bad()) {
    return result;
  }
  else {
    return std::string();
  }
}


int compileVP(std::istream & in,std::ostream & out)
{
  std::string prg = readinput(in);

  if(prg.length() > 0) {
    CVPParser parser;
    CCompiler compiler;
    
    parser.Parse(prg.c_str());
    compiler.Compile(&parser);
    
    struct vertex_program_exec *vpi = compiler.GetInstructions();
    std::list<struct nvfx_relocation> branch_reloc = compiler.GetBranchRelocations();
    for(std::list<struct nvfx_relocation>::iterator it = branch_reloc.begin();it!=branch_reloc.end();it++) {
      struct vertex_program_exec *vpe = &vpi[it->location];
      
      vpe->data[3] &= ~NV40_VP_INST_IADDRL_MASK;
      vpe->data[3] |= (it->target&7) << NV40_VP_INST_IADDRL_SHIFT;
      
      vpe->data[2] &= ~NV40_VP_INST_IADDRH_MASK;
      vpe->data[2] |= ((it->target >> 3)&0x3f) << NV40_VP_INST_IADDRH_SHIFT;
    }
    
    std::list<struct nvfx_relocation> const_reloc = compiler.GetConstRelocations();
    for(std::list<struct nvfx_relocation>::iterator it = const_reloc.begin();it!=const_reloc.end();it++) {
      struct vertex_program_exec *vpe = &vpi[it->location];
      vpe->data[1] &= ~NVFX_VP(INST_CONST_SRC_MASK);
      vpe->data[1] |= (it->target) << NVFX_VP(INST_CONST_SRC_SHIFT);
    }
    
    int n,i;
    u16 magic = ('V'<<8)|'P';
    u32 lastoff = sizeof(rsxVertexProgram);
    unsigned char *vertexprogram = (unsigned char*)calloc(2*1024*1024,1);
    
    rsxVertexProgram *vp = (rsxVertexProgram*)vertexprogram;
    
    vp->magic = SWAP16(magic);
    vp->start_insn = SWAP16(0);
    vp->const_start = SWAP16(0);
    vp->input_mask = SWAP32(compiler.GetInputMask());
    vp->output_mask = SWAP32(compiler.GetOutputMask());

    while(lastoff&3) 
      vertexprogram[lastoff++] = 0;
    
    // Process attributes:
    rsxProgramAttrib *attribs = (rsxProgramAttrib*)(vertexprogram + lastoff);
    
    vp->attrib_off = SWAP32(lastoff);
    
    n = 0;
    std::list<param> params = parser.GetParameters();
    for(std::list<param>::iterator it = params.begin();it!=params.end();it++) {
      if(!it->is_const) {
	it->user = lastoff + (n*sizeof(rsxProgramAttrib));
	attribs[n].index = SWAP32(it->index);
	attribs[n].name_off = SWAP32(0);
	attribs[n].type = it->type;
	attribs[n].is_output = it->is_output;
	n++;
      }
    }
    vp->num_attrib = SWAP16(n);
    lastoff += (n*sizeof(rsxProgramAttrib));
    
    while(lastoff&3)
      vertexprogram[lastoff++] = 0;
    
    // Process constants
    rsxProgramConst *consts = (rsxProgramConst*)(vertexprogram + lastoff);
    
    vp->const_off = SWAP32(lastoff);
    
    n = 0;
    for(std::list<param>::iterator it = params.begin();it!=params.end();it++) {
      if(it->is_const) {
	it->user = lastoff + (n*sizeof(rsxProgramConst));
	consts[n].count = it->count;
	consts[n].type = it->type;
	consts[n].is_internal = it->is_internal;
	consts[n].name_off = SWAP32(0);
	for(i=0;i<(s32)it->count;i++) {
	  f32 *pVal = it->values[i];
	  consts[n].index = SWAP32(it->index + i);
	  consts[n].values[0].f = pVal[0];
	  consts[n].values[0].u = SWAP32(consts[n].values[0].u);
	  consts[n].values[1].f = pVal[1];
	  consts[n].values[1].u = SWAP32(consts[n].values[1].u);
	  consts[n].values[2].f = pVal[2];
	  consts[n].values[2].u = SWAP32(consts[n].values[2].u);
	  consts[n].values[3].f = pVal[3];
	  consts[n].values[3].u = SWAP32(consts[n].values[3].u);
	  n++;
	}
      }
    }
    vp->num_const = SWAP16(n);
    lastoff += (n*sizeof(rsxProgramConst));
    
    while(lastoff&3)
      vertexprogram[lastoff++] = 0;

    // Process parameter names:
    for(std::list<param>::iterator it = params.begin();it!=params.end();it++) {
      if(!it->name.empty() && !it->is_internal) {
	const char *name = it->name.c_str();

	int off = lastoff;
	while(*name) 
	  vertexprogram[lastoff++] = *name++;
	vertexprogram[lastoff++] = 0;
	*((u32*)(vertexprogram + it->user)) = SWAP32(off);
      }
    }
    
    while(lastoff&15)
      vertexprogram[lastoff++] = 0;
    
    // Process microcode:
    vp->ucode_off = SWAP32(lastoff);
    vp->num_insn = SWAP16(compiler.GetInstructionCount());
    
    u32 *dstcodeptr = (u32*)(vertexprogram + lastoff);
    for(i=0,n=0;i<compiler.GetInstructionCount();i++,n+=4,lastoff+=16) {
      dstcodeptr[n+0] = SWAP32(vpi[i].data[0]);
      dstcodeptr[n+1] = SWAP32(vpi[i].data[1]);
      dstcodeptr[n+2] = SWAP32(vpi[i].data[2]);
      dstcodeptr[n+3] = SWAP32(vpi[i].data[3]);

      fprintf(stderr,"%04u: %08x %08x %08x %08x\n",i,
	      SWAP32(dstcodeptr[n + 0]),SWAP32(dstcodeptr[n + 1]),SWAP32(dstcodeptr[n + 2]),SWAP32(dstcodeptr[n + 3]));

      const uint32_t opcode = (vpi[i].data[1] & NV40_VP_INST_VEC_OPCODE_MASK) >> NV40_VP_INST_VEC_OPCODE_SHIFT;
    }

    out.write((const char *)vertexprogram,lastoff);

    if(out.good()) {
      return EXIT_SUCCESS;
    }
    else {
      return EXIT_FAILURE;
    }
  }
  else {
    return EXIT_FAILURE;
  }
}

int compileFP(std::istream & in,std::ostream & out)
{
  std::string prg = readinput(in);

  if(prg.length() > 0) {
    CFPParser parser;
    CCompilerFP compiler;
    
    parser.Parse(prg.c_str());
    compiler.Compile(&parser);
    
    int n,i;
    u16 magic = ('F'<<8)|'P';
    u32 lastoff = sizeof(rsxFragmentProgram);
    unsigned char *fragmentprogram = (unsigned char*)calloc(2*1024*1024,1);
    
    rsxFragmentProgram *fp = (rsxFragmentProgram*)fragmentprogram;
    
    fp->magic = SWAP16(magic);
    fp->num_regs = SWAP32(compiler.GetNumRegs());
    fp->fp_control = SWAP32(compiler.GetFPControl());

    fp->texcoords = SWAP16(compiler.GetTexcoords());
    fp->texcoord2D = SWAP16(compiler.GetTexcoord2D());
    fp->texcoord3D = SWAP16(compiler.GetTexcoord3D());

    while(lastoff&3)
      fragmentprogram[lastoff++] = 0;

    // Process attributes:
    fp->attrib_off = SWAP32(lastoff);
    rsxProgramAttrib *attribs = (rsxProgramAttrib*)(fragmentprogram + lastoff);
    
    n = 0;
    std::list<param> params = parser.GetParameters();
    for(std::list<param>::iterator it = params.begin();it!=params.end();it++) {
      if(!it->is_const /*&& !it->is_output*/) {
	it->user = lastoff + (n*sizeof(rsxProgramAttrib));
	attribs[n].index = SWAP32(it->index);
	attribs[n].name_off = SWAP32(0);
	attribs[n].type = it->type;
	attribs[n].is_output = it->is_output;
	n++;
      }
    }
    fp->num_attrib = SWAP16(n);
    lastoff += (n*sizeof(rsxProgramAttrib));
    
    while(lastoff&3)
      fragmentprogram[lastoff++] = 0;
    
    // Process constant offsets:
    std::list<struct fragment_program_data> const_relocs = compiler.GetConstRelocations();
    for(std::list<param>::iterator it = params.begin();it!=params.end();it++) {
      if(it->is_const && !it->is_internal && !it->is_output) {
	for(i=0;i<it->count;i++) {
	  s32 k = 0;
	  rsxConstOffsetTable *const_table = (rsxConstOffsetTable*)(fragmentprogram + lastoff);
	  
	  const_table->num = SWAP32(0);
	  for(std::list<struct fragment_program_data>::iterator d=const_relocs.begin();d!=const_relocs.end();d++) {
	    if(d->index==(it->index + i)) {
	      const_table->offset[k++] = SWAP32((d->offset*16));
	      d->user = lastoff;
	    }
	  }
	  const_table->num = SWAP32(k);
	  lastoff += (4*k + 4);
	}
      }
    }
    
    while(lastoff&3)
      fragmentprogram[lastoff++] = 0;

    // Process constants:
    fp->const_off = SWAP32(lastoff);
    rsxProgramConst *consts = (rsxProgramConst*)(fragmentprogram + lastoff);
    
    n = 0;
    for(std::list<param>::iterator it = params.begin();it!=params.end();it++) {
      if(it->is_const && !it->is_internal) {
	it->user = lastoff + (n*sizeof(rsxProgramConst));
	
	consts[n].count = it->count;
	consts[n].type = it->type;
	consts[n].is_internal = it->is_internal;
	consts[n].name_off = SWAP32(0);
	
	for(i=0;i<it->count;i++) {
	  s32 table_off = -1;
	  for(std::list<struct fragment_program_data>::iterator d=const_relocs.begin();d!=const_relocs.end();d++) {
	    if(d->index==(it->index + i)) {
	      table_off = d->user;
	      break;
	    }
	  }
	  
	  f32 *pVal = it->values[i];
	  consts[n].index = SWAP32(table_off);
	  consts[n].values[0].f = pVal[0];
	  consts[n].values[0].u = SWAP32(consts[n].values[0].u);
	  consts[n].values[1].f = pVal[1];
	  consts[n].values[1].u = SWAP32(consts[n].values[1].u);
	  consts[n].values[2].f = pVal[2];
	  consts[n].values[2].u = SWAP32(consts[n].values[2].u);
	  consts[n].values[3].f = pVal[3];
	  consts[n].values[3].u = SWAP32(consts[n].values[3].u);
	  n++;
	}
      }
    }
    fp->num_const = SWAP16(n);
    lastoff += (n*sizeof(rsxProgramConst));
    
    while(lastoff&3)
      fragmentprogram[lastoff++] = 0;

    // Process names:
    for(std::list<param>::iterator it = params.begin();it!=params.end();it++) {
      if(!it->name.empty() && !it->is_internal) {
	const char *name = it->name.c_str();

	int off = lastoff;
	while(*name) 
	  fragmentprogram[lastoff++] = *name++;
	fragmentprogram[lastoff++] = 0;
	*((u32*)(fragmentprogram + it->user)) = SWAP32(off);
      }
    }
    
    while(lastoff&15)
      fragmentprogram[lastoff++] = 0;
    
    // Process microcode:
    fp->ucode_off = SWAP32(lastoff);
    fp->num_insn = SWAP16(compiler.GetInstructionCount());
    
    struct fragment_program_exec *fpi = compiler.GetInstructions();
    u32 *dstcodeptr = (u32*)(fragmentprogram + lastoff);
    for(i=0,n=0;i<compiler.GetInstructionCount();i++,n+=4,lastoff+=16) {
      dstcodeptr[n+0] = endian_fp((SWAP32(fpi[i].data[0])));
      dstcodeptr[n+1] = endian_fp((SWAP32(fpi[i].data[1])));
      dstcodeptr[n+2] = endian_fp((SWAP32(fpi[i].data[2])));
      dstcodeptr[n+3] = endian_fp((SWAP32(fpi[i].data[3])));

      fprintf(stderr,"%04u: %08x %08x %08x %08x\n",i,
	      SWAP32(dstcodeptr[n + 0]),SWAP32(dstcodeptr[n + 1]),SWAP32(dstcodeptr[n + 2]),SWAP32(dstcodeptr[n + 3]));
      
      const uint32_t opcode = (fpi[i].data[0] & NVFX_FP_OP_OPCODE_MASK) >> NVFX_FP_OP_OPCODE_SHIFT;
      const uint32_t outreg = (fpi[i].data[0] & NVFX_FP_OP_OUT_REG_MASK) >> NVFX_FP_OP_OUT_REG_SHIFT;

      const uint32_t srcs[4] = {
	(fpi[i].data[0] & NVFX_FP_OP_INPUT_SRC_MASK) >> NVFX_FP_OP_INPUT_SRC_SHIFT,
	(fpi[i].data[1] & NVFX_FP_OP_INPUT_SRC_MASK) >> NVFX_FP_OP_INPUT_SRC_SHIFT,
	(fpi[i].data[2] & NVFX_FP_OP_INPUT_SRC_MASK) >> NVFX_FP_OP_INPUT_SRC_SHIFT,
	(fpi[i].data[3] & NVFX_FP_OP_INPUT_SRC_MASK) >> NVFX_FP_OP_INPUT_SRC_SHIFT
      };
    }
    
    out.write((const char *)fragmentprogram,lastoff);

    if(out.good()) {
      return EXIT_SUCCESS;
    }
    else {
      return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

int main(int argc,char * const argv[])
{
  int opt = -1;

  int type = 'v';

  const char * output_filename = 0;

  while((opt = getopt(argc,argv,"vfo:h")) != -1) {
    // set the program type:
    if(opt == 'v' || opt == 'f') {
      type = opt;
    }
    // write output to a file, instead of to stdout:
    else if(opt == 'o') {
      output_filename = optarg;
    }
    else if(opt == 'h') {
      usage();
      return 0;
    }
    else {
      usage();
      return EXIT_FAILURE;
    }
  };

  argc -= optind;
  argv += optind;

  std::ifstream input_file;
  if(argc > 0) {
    input_file.open(*argv,std::ios::in | std::ios::binary);
    if(!input_file.is_open()) {
      std::cerr << "Failed to open file " << *argv << " for reading" << std::endl;
      return EXIT_FAILURE;
    }
  }

  std::ofstream output_file;
  if(output_filename != 0) {
    output_file.open(output_filename,std::ios::out | std::ios::binary);
    if(!output_file.is_open()) {
      std::cerr << "Failed to open file " << output_filename << " for writing" << std::endl;
    }
  }

  if(type == 'v') {
    return compileVP((argc > 0) ? input_file : std::cin,
		     (output_filename != 0) ? output_file : std::cout);
  }
  else if(type == 'f') {
    return compileFP((argc > 0) ? input_file : std::cin,
		     (output_filename != 0) ? output_file : std::cout);
  }
  else {
    return EXIT_FAILURE;
  }

  return 0;
}
