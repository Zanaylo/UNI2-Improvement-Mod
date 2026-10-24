#include "Hooks/CallSiteScan.h"

#include "../depends/minhook/src/hde/hde32.h"

#include <cstring>

namespace {

constexpr uint8_t kCallRel32 = 0xE8;
constexpr uint8_t kGroupFF = 0xFF;
constexpr uint8_t kModrmCallAbsolute = 0x15;
constexpr uint8_t kModrmRegJmp = 4;
constexpr uint8_t kRet = 0xC3;
constexpr uint8_t kRetImm16 = 0xC2;
constexpr uint8_t kJmpRel32 = 0xE9;
constexpr uint8_t kJmpRel8 = 0xEB;
constexpr uint8_t kPadding = 0xCC;
constexpr size_t kCallRel32Bytes = 5;
constexpr size_t kCallAbsoluteBytes = 6;

bool LeavesFunction(const hde32s& instruction)
{
	if (instruction.opcode == kRet || instruction.opcode == kRetImm16)
		return true;

	if (instruction.opcode == kJmpRel32 || instruction.opcode == kJmpRel8)
		return true;

	return instruction.opcode == kGroupFF && instruction.modrm_reg == kModrmRegJmp;
}

bool IsDirectCall(const hde32s& instruction)
{
	return instruction.opcode == kCallRel32 && instruction.len == kCallRel32Bytes && instruction.p_66 == 0;
}

bool IsImportCall(const hde32s& instruction)
{
	return instruction.opcode == kGroupFF && instruction.modrm == kModrmCallAbsolute &&
		instruction.len == kCallAbsoluteBytes && (instruction.flags & F_PREFIX_ANY) == 0;
}

CallSiteScan::Site MakeSite(const hde32s& instruction, size_t offset, uintptr_t address)
{
	CallSiteScan::Site site = {};
	site.offset = offset;
	site.length = instruction.len;

	if (IsDirectCall(instruction))
	{
		site.kind = CallSiteScan::Kind_Direct;
		site.operand = address + offset + kCallRel32Bytes + static_cast<int32_t>(instruction.imm.imm32);
		return site;
	}

	site.kind = CallSiteScan::Kind_Import;
	site.operand = instruction.disp.disp32;
	return site;
}

}

int CallSiteScan::Find(const uint8_t* code, uintptr_t address, Site* out, int capacity)
{
	if (code == nullptr || out == nullptr || capacity <= 0)
		return -1;

	int count = 0;
	size_t offset = 0;

	while (offset < kLongestFunction)
	{
		hde32s instruction;
		memset(&instruction, 0, sizeof(instruction));

		if (hde32_disasm(code + offset, &instruction) == 0 || (instruction.flags & F_ERROR) != 0)
			return -1;

		if (IsDirectCall(instruction) || IsImportCall(instruction))
		{
			if (count >= capacity)
				return -1;

			out[count++] = MakeSite(instruction, offset, address);
		}

		offset += instruction.len;

		if (LeavesFunction(instruction) && code[offset] == kPadding)
			return count;
	}

	return -1;
}
