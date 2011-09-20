#ifndef __COMPILE_H__
#define __COMPILE_H__

struct vertex_program_exec
{
	u32 data[4];
};

struct vertex_program_data
{
	s32 index;
	f32 value[4];
};

class CParser;

class CCompiler
{
public:
	CCompiler();
	virtual ~CCompiler();

	void Compile(CParser *pParser);

	int GetInputMask() const {return m_nInputMask;}
	int GetOutputMask() const {return m_nOutputMask;}
	int GetNumRegs() const { return m_nNumRegs; }
	int GetInstructionCount() const { return m_nInstructions; }
	struct vertex_program_exec* GetInstructions() const { return m_pInstructions;}
	std::list<struct nvfx_relocation> GetConstRelocations() const { return m_lConstRelocation; }
	std::list<struct nvfx_relocation> GetBranchRelocations() const { return m_lBranchRelocation; }

private:
	void Prepare(CParser *pParser);

	void emit_insn(u8 opcode,struct nvfx_insn *insn);
	void emit_dst(u32 *hw,u8 slot,struct nvfx_insn *insn);
	void emit_src(u32 *hw,u8 pos,struct nvfx_src *src);

	struct nvfx_reg temp();
	struct nvfx_reg constant(s32 pipe,f32 x,f32 y,f32 z,f32 w);

	void release_temps();

	int m_nNumRegs;
	int m_nInputMask;
	int m_nOutputMask;

	int m_nInstructions;
	int m_nCurInstruction;
	struct vertex_program_exec *m_pInstructions;

	int m_nConsts;
	struct vertex_program_data *m_pConstData;

	int m_rTemps;
	int m_rTempsDiscard;

	struct nvfx_reg *m_rTemp;
	struct nvfx_reg *m_rConst;

	std::list<struct nvfx_relocation> m_lConstRelocation;
	std::list<struct nvfx_relocation> m_lBranchRelocation;
};

#endif
