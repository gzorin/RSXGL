#ifndef __VPPARSER_H__
#define __VPPARSER_H__

#include "parser.h"

#define NV_OPTION_VP1		0x01
#define NV_OPTION_VP2		0x02
#define NV_OPTION_VP3		0x04

class CVPParser : public CParser
{
public:
	CVPParser();
	virtual ~CVPParser();

	virtual int Parse(const char *str);

private:
	void ParseInstruction(struct nvfx_insn *insn,opcode *opc,const char *param_str);
	void ParseMaskedDstReg(const char *token,struct nvfx_insn *insn);

	void ParseMaskedDstAddr(const char *token,struct nvfx_insn *insn);
	void ParseSwizzledSrcReg(const char *token,struct nvfx_src *reg);

	const char* ParseOutputReg(const char *token,s32 *reg);
	const char* ParseInputReg(const char *token,s32 *reg);
	const char* ParseParamReg(const char *token,struct nvfx_src *reg);

	opcode* FindOpcode(const char *mnemonic);

	virtual s32 ConvertInputReg(const char *token);
	virtual const char* ParseOutputMask(const char *token,u8 *mask);

};

#endif
