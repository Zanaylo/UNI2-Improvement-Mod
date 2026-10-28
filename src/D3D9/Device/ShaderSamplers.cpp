#include "D3D9/Device/ShaderSamplers.h"

namespace {

constexpr uint32_t kEndToken = 0x0000ffffu;
constexpr uint32_t kOpcodeMask = 0xffffu;
constexpr uint32_t kCommentOpcode = 0xfffeu;
constexpr uint32_t kCommentLengthShift = 16;
constexpr uint32_t kCommentLengthMask = 0x7fffu;
constexpr uint32_t kInstructionLengthShift = 24;
constexpr uint32_t kInstructionLengthMask = 0xfu;
constexpr uint32_t kDeclareOpcode = 31;
constexpr size_t kDeclareRegisterOperand = 2;
constexpr uint32_t kRegisterTypeLowShift = 28;
constexpr uint32_t kRegisterTypeLowMask = 0x7u;
constexpr uint32_t kRegisterTypeHighShift = 8;
constexpr uint32_t kRegisterTypeHighMask = 0x18u;
constexpr uint32_t kRegisterNumberMask = 0x7ffu;
constexpr uint32_t kSamplerRegister = 10;
constexpr uint32_t kVersionMajorShift = 8;
constexpr uint32_t kVersionMajorMask = 0xffu;
constexpr uint32_t kFirstModelWithDeclarations = 2;
constexpr uint32_t kStageBits = 32;
constexpr uint32_t kDisabledOp = 1;

uint32_t RegisterType(uint32_t operand)
{
	return ((operand >> kRegisterTypeLowShift) & kRegisterTypeLowMask) |
		((operand >> kRegisterTypeHighShift) & kRegisterTypeHighMask);
}

uint32_t SamplerBit(uint32_t operand)
{
	if (RegisterType(operand) != kSamplerRegister)
		return 0;

	const uint32_t number = operand & kRegisterNumberMask;
	return number < kStageBits ? 1u << number : 0;
}

bool DeclaresWithTokens(uint32_t version)
{
	return ((version >> kVersionMajorShift) & kVersionMajorMask) >= kFirstModelWithDeclarations;
}

size_t Length(uint32_t token)
{
	if ((token & kOpcodeMask) == kCommentOpcode)
		return (token >> kCommentLengthShift) & kCommentLengthMask;

	return (token >> kInstructionLengthShift) & kInstructionLengthMask;
}

}

uint32_t ShaderSamplers::Declared(const uint32_t* function, size_t tokens)
{
	if (function == nullptr || tokens == 0 || !DeclaresWithTokens(function[0]))
		return kEveryStage;

	uint32_t declared = 0;

	for (size_t at = 1; at < tokens;)
	{
		const uint32_t token = function[at];

		if (token == kEndToken)
			return declared;

		const size_t length = Length(token);

		if (at + length >= tokens)
			return kEveryStage;

		if ((token & kOpcodeMask) == kDeclareOpcode && length >= kDeclareRegisterOperand)
			declared |= SamplerBit(function[at + kDeclareRegisterOperand]);

		at += 1 + length;
	}

	return kEveryStage;
}

uint32_t ShaderSamplers::Enabled(const uint32_t* colourOps, size_t stages)
{
	uint32_t enabled = 0;

	for (size_t stage = 0; stage < stages && stage < kStageBits; ++stage)
	{
		if (colourOps[stage] == kDisabledOp)
			return enabled;

		enabled |= 1u << stage;
	}

	return enabled;
}
