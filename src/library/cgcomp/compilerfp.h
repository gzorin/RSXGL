#ifndef __COMPILERFP_H__
#define __COMPILERFP_H__

class CParser;

struct fragment_program_exec
{
	u32 data[4];
};

struct fragment_program_data
{
	u32 offset;
	s32 index;
	s32 user;
};

class CCompilerFP
{
public:
	CCompilerFP();
	virtual ~CCompilerFP();

	void Compile(CParser *pParser);

	int GetNumRegs() const { return m_nNumRegs; }
	int GetFPControl() const { return m_nFPControl; }
	int GetTexcoords() const { return m_nTexcoords; }
	int GetTexcoord2D() const { return m_nTexcoord2D; }
	int GetTexcoord3D() const { return m_nTexcoord3D; }

	int GetInstructionCount() const {return m_nInstructions;}
	std::list<struct fragment_program_data> GetConstRelocations() const { return m_lConstData; }
	struct fragment_program_exec* GetInstructions() const { return m_pInstructions; }

private:
	void Prepare(CParser *pParser);

	void emit_insn(u8 op,struct nvfx_insn *insn);
	void emit_dst(struct nvfx_reg *dst,bool *have_const);
	void emit_src(s32 pos,struct nvfx_src *src,bool *have_const);
	void emit_brk(struct nvfx_insn *insn);
	void emit_rep(struct nvfx_insn *insn);
	void emit_if(struct nvfx_insn *insn);
	void fixup_rep();
	void fixup_if();
	void fixup_else();

	void grow_insns(int count);

	struct nvfx_reg temp();
	void release_temps();

	inline param GetImmData(int index)
	{
		s32 i;
		std::list<param>::iterator it = m_lParameters.begin();
		for(;it!=m_lParameters.end();it++) {
			for(i=0;i<it->count;i++) {
				if((int)(it->index + i)==index) {
					if(it->is_const && it->is_internal)
						return *it;
				}
			}
		}
		return param();
	}

	inline param GetInputAttrib(int index)
	{
		s32 i;
		std::list<param>::iterator it = m_lParameters.begin();
		for(;it!=m_lParameters.end();it++) {
			for(i=0;i<it->count;i++) {
				if((int)(it->index + i)==index) {
					if(!it->is_const && !it->is_internal)
						return *it;
				}
			}
		}
		return param();
	}

	int m_nNumRegs;
	int m_nFPControl;
	int m_nSamplers;
	int m_nTexcoords;
	int m_nTexcoord2D;
	int m_nTexcoord3D;
	int m_nInstructions;
	int m_nCurInstruction;
	struct fragment_program_exec *m_pInstructions;

	int m_rTemps;
	int m_rTempsDiscard;

	struct nvfx_reg *m_rTemp;

	std::list<param> m_lParameters;
	std::list<struct fragment_program_data> m_lConstData;
	std::stack<int> m_repStack;
	std::stack<int> m_ifStack;
};

#endif
