#ifndef __FPPARSER_H__
#define __FPPARSER_H__

#include "parser.h"

#define NV_OPTION_FP1		0x01
#define NV_OPTION_FP2		0x02
#define NV_OPTION_FP3		0x04

/**
 * Bit flags for each type of texture object
 * Used for Texture.Unit[]._ReallyEnabled flags.
 */
/*@{*/
#define TEXTURE_2D_ARRAY_BIT (1 << TEXTURE_2D_ARRAY_INDEX)
#define TEXTURE_1D_ARRAY_BIT (1 << TEXTURE_1D_ARRAY_INDEX)
#define TEXTURE_CUBE_BIT     (1 << TEXTURE_CUBE_INDEX)
#define TEXTURE_3D_BIT       (1 << TEXTURE_3D_INDEX)
#define TEXTURE_RECT_BIT     (1 << TEXTURE_RECT_INDEX)
#define TEXTURE_2D_BIT       (1 << TEXTURE_2D_INDEX)
#define TEXTURE_1D_BIT       (1 << TEXTURE_1D_INDEX)
/*@}*/

typedef struct _oparam
{
	std::string alias;
	s32 index;
} oparam;

typedef enum
{
   TEXTURE_2D_ARRAY_INDEX,
   TEXTURE_1D_ARRAY_INDEX,
   TEXTURE_CUBE_INDEX,
   TEXTURE_3D_INDEX,
   TEXTURE_RECT_INDEX,
   TEXTURE_2D_INDEX,
   TEXTURE_1D_INDEX,
   NUM_TEXTURE_TARGETS
} texture_index;


class CFPParser : public CParser
{
public:
	CFPParser();
	virtual ~CFPParser();

	virtual int Parse(const char *str);

private:
	void ParseInstruction(struct nvfx_insn *insn,opcode *opc,const char *param_str);
	void ParseMaskedDstReg(const char *token,struct nvfx_insn *insn);
	void ParseVectorSrc(const char *token,struct nvfx_src *reg);
	void ParseScalarSrc(const char *token,struct nvfx_src *reg);
	void ParseTextureUnit(const char *token,u8 *texUnit);
	void ParseTextureTarget(const char *token,u8 *texTarget);

	void ParseOutput(const char *param_str);

	const char* ParseOutputReg(const char *token,s32 *reg);
	const char* ParseInputReg(const char *token,s32 *reg);
	const char* ParseOutputRegAlias(const char *token,s32 *reg);

	opcode FindOpcode(const char *mnemonic);

	int GetConstRegType(int index);

	virtual s32 ConvertInputReg(const char *token);
	virtual const char* ParseOutputMask(const char *token,u8 *mask);

	std::list<oparam> m_lOParameters;
};

#endif
