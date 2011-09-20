#include "types.h"

#include "parser.h"
#include "compilerfp.h"

#define arith(s,d,m,s0,s1,s2) \
	nvfx_insn((s),0,-1,(d),(m),(s0),(s1),(s2))

#define arith_ctor(ins,d,s0,s1,s2) \
	nvfx_insn_ctor((ins),(d),(s0),(s1),(s2))

static INLINE s32 ffs(u32 u)
{
	u32 i = 0;
	if(!(u&0xffffffff)) return 0;
	while(!(u&0x1)) {
		u >>= 1;
		i++;
	}
	return i + 1;
}

CCompilerFP::CCompilerFP()
{
	m_rTemps = 0;
	m_nNumRegs = 2;
	m_nSamplers = 0;
	m_nFPControl = 0;
	m_nTexcoords = 0;
	m_nTexcoord2D = 0;
	m_nTexcoord3D = 0;
	m_nInstructions = 0;
	m_rTempsDiscard = 0;
	m_nCurInstruction = 0;
	m_pInstructions = NULL;
	m_rTemp = NULL;
}

CCompilerFP::~CCompilerFP()
{
}

void CCompilerFP::Prepare(CParser *pParser)
{
	int high_temp = -1;
	int i,j,nCount = pParser->GetInstructionCount();
	struct nvfx_insn *insns = pParser->GetInstructions();

	m_lParameters = pParser->GetParameters();

	for(i=0;i<nCount;i++) {
		struct nvfx_insn *insn = &insns[i];

		for(j=0;j<3;j++) {
			struct nvfx_src *src = &insn->src[j];

			switch(src->reg.type) {
				case NVFXSR_INPUT:
					break;
				case NVFXSR_TEMP:
					if((s32)src->reg.index>high_temp) high_temp = src->reg.index;
					break;
			}
		}

		switch(insn->dst.type) {
			case NVFXSR_TEMP:
				if((s32)insn->dst.index>high_temp) high_temp = insn->dst.index;
				break;
		}
	}

	if(++high_temp) {
		m_rTemp = (struct nvfx_reg*)calloc(high_temp,sizeof(struct nvfx_reg));
		for(i=0;i<high_temp;i++) m_rTemp[i] = temp();
		m_rTempsDiscard = 0;
	}
}

void CCompilerFP::Compile(CParser *pParser)
{
	int i,nCount = pParser->GetInstructionCount();
	struct nvfx_insn tmp_insn,*insns = pParser->GetInstructions();
	struct nvfx_src tmp,none = nvfx_src(nvfx_reg(NVFXSR_NONE,0));

	Prepare(pParser);

	for(i=0;i<nCount;i++) {
		struct nvfx_insn *insn = &insns[i];

		switch(insn->op) {
			case OPCODE_ADD:
				emit_insn(NVFX_FP_OP_OPCODE_ADD,insn);
				break;
			case OPCODE_BRK:
				emit_brk(insn);
				break;
			case OPCODE_COS:
				emit_insn(NVFX_FP_OP_OPCODE_COS,insn);
				break;
			case OPCODE_DP3:
				emit_insn(NVFX_FP_OP_OPCODE_DP3,insn);
				break;
			case OPCODE_DP4:
				emit_insn(NVFX_FP_OP_OPCODE_DP4,insn);
				break;
			case OPCODE_EX2:
				emit_insn(NVFX_FP_OP_OPCODE_EX2,insn);
				break;
			case OPCODE_LG2:
				emit_insn(NVFX_FP_OP_OPCODE_LG2,insn);
				break;
			case OPCODE_LRP:
				tmp = nvfx_src(temp());
				tmp_insn = arith(0,tmp.reg,insn->mask,neg(insn->src[0]),insn->src[2],insn->src[2]);
				emit_insn(NVFX_FP_OP_OPCODE_MAD,&tmp_insn);

				tmp_insn = arith(insn->sat,insn->dst,insn->mask,insn->src[0],insn->src[1],tmp);
				emit_insn(NVFX_FP_OP_OPCODE_MAD,&tmp_insn);
				break;
			case OPCODE_MAX:
				emit_insn(NVFX_FP_OP_OPCODE_MAX,insn);
				break;
			case OPCODE_MIN:
				emit_insn(NVFX_FP_OP_OPCODE_MIN,insn);
				break;
			case OPCODE_MAD:
				emit_insn(NVFX_FP_OP_OPCODE_MAD,insn);
				break;
			case OPCODE_MOV:
				emit_insn(NVFX_FP_OP_OPCODE_MOV,insn);
				break;
			case OPCODE_MUL:
				emit_insn(NVFX_FP_OP_OPCODE_MUL,insn);
				break;
			case OPCODE_POW:
				tmp = nvfx_src(temp());

				tmp_insn = arith(0,tmp.reg, NVFX_FP_MASK_X, insn->src[0], none, none);
				emit_insn(NVFX_FP_OP_OPCODE_LG2,&tmp_insn);

				tmp_insn = arith(0,tmp.reg, NVFX_FP_MASK_X, swz(tmp, X, X, X, X),insn->src[1], none);
				emit_insn(NVFX_FP_OP_OPCODE_MUL,&tmp_insn);

				tmp_insn = arith_ctor(insn,insn->dst,swz(tmp, X, X, X, X), none, none);
				emit_insn(NVFX_FP_OP_OPCODE_EX2,&tmp_insn);
				break;
			case OPCODE_RCP:
				emit_insn(NVFX_FP_OP_OPCODE_RCP,insn);
				break;
			case OPCODE_RSQ:
				tmp = nvfx_src(temp());
				tmp_insn = arith(0,tmp.reg,NVFX_FP_MASK_X,abs(insn->src[0]),none,none);
				tmp_insn.scale = NVFX_FP_OP_DST_SCALE_INV_2X;
				emit_insn(NVFX_FP_OP_OPCODE_LG2,&tmp_insn);

				tmp_insn = arith_ctor(insn,insn->dst,neg(swz(tmp,X,X,X,X)),none,none);
				emit_insn(NVFX_FP_OP_OPCODE_EX2,&tmp_insn);
				break;
			case OPCODE_SEQ:
				emit_insn(NVFX_FP_OP_OPCODE_SEQ,insn);
				break;
			case OPCODE_SFL:
				emit_insn(NVFX_FP_OP_OPCODE_SFL,insn);
				break;
			case OPCODE_SGE:
				emit_insn(NVFX_FP_OP_OPCODE_SGE,insn);
				break;
			case OPCODE_SGT:
				emit_insn(NVFX_FP_OP_OPCODE_SGT,insn);
				break;
			case OPCODE_SIN:
				emit_insn(NVFX_FP_OP_OPCODE_SIN,insn);
				break;
			case OPCODE_SLE:
				emit_insn(NVFX_FP_OP_OPCODE_SLE,insn);
				break;
			case OPCODE_SLT:
				emit_insn(NVFX_FP_OP_OPCODE_SLT,insn);
				break;
			case OPCODE_SNE:
				emit_insn(NVFX_FP_OP_OPCODE_SNE,insn);
				break;
			case OPCODE_TEX:
				emit_insn(NVFX_FP_OP_OPCODE_TEX,insn);
				break;
			case OPCODE_TXB:
				emit_insn(NVFX_FP_OP_OPCODE_TXB,insn);
				break;
			case OPCODE_TXL:
				emit_insn(NVFX_FP_OP_OPCODE_TXL_NV40,insn);
				break;
			case OPCODE_TXP:
				emit_insn(NVFX_FP_OP_OPCODE_TXP,insn);
				break;
			case OPCODE_BGNREP:
				emit_rep(insn);
				break;
			case OPCODE_ENDREP:
				fixup_rep();
				break;
			case OPCODE_IF:
				emit_if(insn);
				break;
			case OPCODE_ENDIF:
				fixup_if();
				break;
			case OPCODE_ELSE:
				fixup_else();
				break;
			case OPCODE_END:
				if(m_nInstructions) m_pInstructions[m_nCurInstruction].data[0] |= NVFX_FP_OP_PROGRAM_END;
				else {
					m_nCurInstruction = m_nInstructions;
					grow_insns(1);
					m_pInstructions[m_nCurInstruction].data[0] = 0x00000001;
					m_pInstructions[m_nCurInstruction].data[1] = 0x00000000;
					m_pInstructions[m_nCurInstruction].data[2] = 0x00000000;
					m_pInstructions[m_nCurInstruction].data[3] = 0x00000000;
				}
		}
		release_temps();
	}
}

void CCompilerFP::emit_insn(u8 op,struct nvfx_insn *insn)
{
	u32 *hw;
	bool have_const = false;

	m_nCurInstruction = m_nInstructions;
	grow_insns(1);
	memset(&m_pInstructions[m_nCurInstruction],0,sizeof(struct fragment_program_exec));

	hw = m_pInstructions[m_nCurInstruction].data;

	if(op==NVFX_FP_OP_OPCODE_KIL)
		m_nFPControl |= NV30_3D_FP_CONTROL_USES_KIL;

	hw[0] |= (op << NVFX_FP_OP_OPCODE_SHIFT);
	hw[0] |= (insn->mask << NVFX_FP_OP_OUTMASK_SHIFT);
	hw[0] |= (insn->precision << NVFX_FP_OP_PRECISION_SHIFT);
	hw[2] |= (insn->scale << NVFX_FP_OP_DST_SCALE_SHIFT);

	if(insn->sat)
		hw[0] |= NVFX_FP_OP_OUT_SAT;

	if(insn->cc_update)
		hw[0] |= NVFX_FP_OP_COND_WRITE_ENABLE;

	hw[1] |= (insn->cc_cond << NVFX_FP_OP_COND_SHIFT);
	hw[1] |= ((insn->cc_swz[0] << NVFX_FP_OP_COND_SWZ_X_SHIFT) |
			  (insn->cc_swz[1] << NVFX_FP_OP_COND_SWZ_Y_SHIFT) |
		      (insn->cc_swz[2] << NVFX_FP_OP_COND_SWZ_Z_SHIFT) |
		      (insn->cc_swz[3] << NVFX_FP_OP_COND_SWZ_W_SHIFT));

	if(insn->unit >= 0) {
		hw[0] |= (insn->unit << NVFX_FP_OP_TEX_UNIT_SHIFT);
		m_nSamplers |= (1<<insn->unit);
	}

	emit_dst(&insn->dst,&have_const);
	emit_src(0,&insn->src[0],&have_const);
	emit_src(1,&insn->src[1],&have_const);
	emit_src(2,&insn->src[2],&have_const);
}

void CCompilerFP::emit_dst(struct nvfx_reg *dst,bool *have_const)
{
	s32 index;
	u32 *hw = m_pInstructions[m_nCurInstruction].data;

	index = dst->index;
	switch(dst->type) {
		case NVFXSR_TEMP:
			if(m_nNumRegs<(s32)(index + 1))
				m_nNumRegs = (index + 1);
			break;
		case NVFXSR_OUTPUT:
			if(dst->index==1)
				m_nFPControl |= 0xe;
			else
				hw[0] |= NVFX_FP_OP_OUT_REG_HALF;
			break;
		case NVFXSR_NONE:
			hw[0] |= NV40_FP_OP_OUT_NONE;
			break;
	}
	if(dst->is_fp16)
		hw[0] |= NVFX_FP_OP_OUT_REG_HALF;

	hw[0] |= (index << NVFX_FP_OP_OUT_REG_SHIFT);
}

void CCompilerFP::emit_src(s32 pos,struct nvfx_src *src,bool *have_const)
{
	u32 sr = 0;
	u32 *hw = m_pInstructions[m_nCurInstruction].data;

	switch(src->reg.type) {
		case NVFXSR_INPUT:
			sr |= (NVFX_FP_REG_TYPE_INPUT << NVFX_FP_REG_TYPE_SHIFT);
			hw[0] |= (src->reg.index << NVFX_FP_OP_INPUT_SRC_SHIFT);

			if(src->reg.index>=NVFX_FP_OP_INPUT_SRC_TC(0) && src->reg.index<=NVFX_FP_OP_INPUT_SRC_TC(8)) {
				param fpi = GetInputAttrib(src->reg.index);

				if((int)fpi.index!=-1) {
					if(fpi.type>PARAM_FLOAT2)
						m_nTexcoord3D |= (1 << (src->reg.index - NVFX_FP_OP_INPUT_SRC_TC0));
					else
						m_nTexcoord2D |= (1 << (src->reg.index - NVFX_FP_OP_INPUT_SRC_TC0));
				}
				m_nTexcoords |= (1 << (src->reg.index - NVFX_FP_OP_INPUT_SRC_TC0));
			}
			break;
		case NVFXSR_TEMP:
			sr |= (NVFX_FP_REG_TYPE_TEMP << NVFX_FP_REG_TYPE_SHIFT);
			sr |= (src->reg.index << NVFX_FP_REG_SRC_SHIFT);
			break;
		case NVFXSR_IMM:
			if(!*have_const) {
				grow_insns(1);
				hw = m_pInstructions[m_nCurInstruction].data;
				*have_const = true;
			}
			{
				param fpd = GetImmData(src->reg.index);
				if(fpd.values!=NULL) memcpy(&m_pInstructions[m_nCurInstruction + 1],fpd.values,4*sizeof(f32));

				sr |= (NVFX_FP_REG_TYPE_CONST << NVFX_FP_REG_TYPE_SHIFT);
			}
			break;
		case NVFXSR_CONST:
			if(!*have_const) {
				grow_insns(1);
				hw = m_pInstructions[m_nCurInstruction].data;
				*have_const = true;
			}
			{
				struct fragment_program_data fpd;

				fpd.offset = m_nCurInstruction + 1;
				fpd.index = src->reg.index;
				fpd.user = -1;
				memset(&m_pInstructions[fpd.offset],0,sizeof(struct fragment_program_exec));
				m_lConstData.push_back(fpd);
			}
			sr |= (NVFX_FP_REG_TYPE_CONST << NVFX_FP_REG_TYPE_SHIFT);
			break;
		case NVFXSR_NONE:
			sr |= (NVFX_FP_REG_TYPE_TEMP << NVFX_FP_REG_TYPE_SHIFT);
			break;
		case NVFXSR_OUTPUT:
			fprintf(stderr,"Output register used as input.\n");
			exit(EXIT_FAILURE);
			return;
	}

	if(src->reg.is_fp16)
		sr |= NVFX_FP_REG_SRC_HALF;

	if(src->negate)
		sr |= NVFX_FP_REG_NEGATE;

	if(src->abs)
		hw[1] |= (1 << (29 + pos));

	sr |= ((src->swz[0] << NVFX_FP_REG_SWZ_X_SHIFT) |
	       (src->swz[1] << NVFX_FP_REG_SWZ_Y_SHIFT) |
	       (src->swz[2] << NVFX_FP_REG_SWZ_Z_SHIFT) |
	       (src->swz[3] << NVFX_FP_REG_SWZ_W_SHIFT));

	hw[pos + 1] |= sr;
}

void CCompilerFP::emit_rep(struct nvfx_insn *insn)
{
	u32 *hw;
	int count;

	m_nCurInstruction = m_nInstructions;
	grow_insns(1);
	memset(&m_pInstructions[m_nCurInstruction],0,sizeof(struct fragment_program_exec));

	hw = m_pInstructions[m_nCurInstruction].data;

	param fpd = GetImmData(insn->src[0].reg.index);

	if (insn->src[0].reg.type != NVFXSR_IMM ||
		(*fpd.values)[0] < 0.0 || (*fpd.values)[0] > 255.0) {
		fprintf(stderr,"Input to REP must be immediate number 0-255\n");
		exit(EXIT_FAILURE);
	}

	count = (int)(*fpd.values)[0];
	hw[0] |= (NV40_FP_OP_BRA_OPCODE_REP << NVFX_FP_OP_OPCODE_SHIFT);
	hw[0] |= NV40_FP_OP_OUT_NONE;
	hw[0] |= NVFX_FP_PRECISION_FP16 <<  NVFX_FP_OP_PRECISION_SHIFT;
	hw[2] |= NV40_FP_OP_OPCODE_IS_BRANCH;
	hw[2] |= (count<<NV40_FP_OP_REP_COUNT1_SHIFT) |
		     (count<<NV40_FP_OP_REP_COUNT2_SHIFT) |
			 (count<<NV40_FP_OP_REP_COUNT3_SHIFT);
	hw[1] |= (insn->cc_cond << NVFX_FP_OP_COND_SHIFT);
	hw[1] |= ((insn->cc_swz[0] << NVFX_FP_OP_COND_SWZ_X_SHIFT) |
		      (insn->cc_swz[1] << NVFX_FP_OP_COND_SWZ_Y_SHIFT) |
			  (insn->cc_swz[2] << NVFX_FP_OP_COND_SWZ_Z_SHIFT) |
			  (insn->cc_swz[3] << NVFX_FP_OP_COND_SWZ_W_SHIFT));

	m_repStack.push(m_nCurInstruction);
}

void CCompilerFP::fixup_rep()
{
	u32 *hw;

	hw = m_pInstructions[m_repStack.top()].data;

	hw[3] |= m_nInstructions * 4;

	m_repStack.pop();
}

void CCompilerFP::fixup_if()
{
	u32 *hw;

	hw = m_pInstructions[m_ifStack.top()].data;

	if(!hw[2]) hw[2] = (NV40_FP_OP_OPCODE_IS_BRANCH | (m_nInstructions*4));
	hw[3] = (m_nInstructions*4);

	m_ifStack.pop();
}

void CCompilerFP::fixup_else()
{
	u32 *hw;

	hw = m_pInstructions[m_ifStack.top()].data;
	hw[2] = (NV40_FP_OP_OPCODE_IS_BRANCH | (m_nInstructions*4));
}

void CCompilerFP::emit_brk(struct nvfx_insn *insn)
{
	u32 *hw;

	m_nCurInstruction = m_nInstructions;
	grow_insns(1);
	memset(&m_pInstructions[m_nCurInstruction],0,sizeof(struct fragment_program_exec));

	hw = m_pInstructions[m_nCurInstruction].data;

	hw[0] |= (NV40_FP_OP_BRA_OPCODE_BRK << NVFX_FP_OP_OPCODE_SHIFT);
	hw[0] |= NV40_FP_OP_OUT_NONE;
	hw[2] |= NV40_FP_OP_OPCODE_IS_BRANCH;

	hw[1] |= (insn->cc_cond << NVFX_FP_OP_COND_SHIFT);
	hw[1] |= ((insn->cc_swz[0] << NVFX_FP_OP_COND_SWZ_X_SHIFT) |
		      (insn->cc_swz[1] << NVFX_FP_OP_COND_SWZ_Y_SHIFT) |
			  (insn->cc_swz[2] << NVFX_FP_OP_COND_SWZ_Z_SHIFT) |
			  (insn->cc_swz[3] << NVFX_FP_OP_COND_SWZ_W_SHIFT));
}

void CCompilerFP::emit_if(struct nvfx_insn *insn)
{
	u32 *hw;

	m_nCurInstruction = m_nInstructions;
	grow_insns(1);
	memset(&m_pInstructions[m_nCurInstruction],0,sizeof(struct fragment_program_exec));

	hw = m_pInstructions[m_nCurInstruction].data;

	hw[0] |= (NV40_FP_OP_BRA_OPCODE_IF << NVFX_FP_OP_OPCODE_SHIFT);
	hw[0] |= NV40_FP_OP_OUT_NONE;
	hw[0] |= (NVFX_FP_PRECISION_FP16 <<  NVFX_FP_OP_PRECISION_SHIFT);

	hw[1] |= (insn->cc_cond << NVFX_FP_OP_COND_SHIFT);
	hw[1] |= ((insn->cc_swz[0] << NVFX_FP_OP_COND_SWZ_X_SHIFT) |
		      (insn->cc_swz[1] << NVFX_FP_OP_COND_SWZ_Y_SHIFT) |
			  (insn->cc_swz[2] << NVFX_FP_OP_COND_SWZ_Z_SHIFT) |
			  (insn->cc_swz[3] << NVFX_FP_OP_COND_SWZ_W_SHIFT));

	m_ifStack.push(m_nCurInstruction);
}

struct nvfx_reg CCompilerFP::temp()
{
	s32 idx = ffs(~m_rTemps) - 1;

	if(idx<0) return nvfx_reg(NVFXSR_TEMP,0);

	m_rTemps |= (1<<idx);
	m_rTempsDiscard |= (1<<idx);

	return nvfx_reg(NVFXSR_TEMP,idx);
}

void CCompilerFP::release_temps()
{
	m_rTemps &= ~m_rTempsDiscard;
	m_rTempsDiscard = 0;
}

void CCompilerFP::grow_insns(int count)
{
	m_nInstructions += count;
	m_pInstructions = (struct fragment_program_exec*)realloc(m_pInstructions,m_nInstructions*sizeof(struct fragment_program_exec));
}
