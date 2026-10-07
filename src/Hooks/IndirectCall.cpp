#include "Hooks/IndirectCall.h"

#include <cstring>

namespace {

constexpr uint8_t kGroupFiveOpcode = 0xFF;
constexpr uint8_t kCallAbsoluteModRm = 0x15;

}

bool IndirectCall::Decode(const uint8_t* code, uint32_t& outSlot)
{
	if (code == nullptr || code[0] != kGroupFiveOpcode || code[1] != kCallAbsoluteModRm)
		return false;

	memcpy(&outSlot, code + kOperandOffset, sizeof(outSlot));
	return true;
}
