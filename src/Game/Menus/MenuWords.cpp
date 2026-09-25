#include "Game/Menus/MenuWords.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"

#include <cstdint>

namespace {

bool ReadPointer(uintptr_t address, uint32_t& out)
{
	return address != 0 && TryReadDword(reinterpret_cast<const void*>(address), out) && out != 0;
}

}

bool MenuWords::Copy(int index, char* out, size_t size)
{
	if (out == nullptr || size == 0 || index < 0)
		return false;

	out[0] = '\0';

	uint32_t vector = 0;
	uint32_t begin = 0;
	uint32_t end = 0;
	uint32_t word = 0;

	if (!ReadPointer(RvaToAddress(GameOffsets::kMenuWords), vector) || !ReadPointer(vector, begin) ||
		!ReadPointer(vector + 4, end) || index >= static_cast<int>((end - begin) / 4) ||
		!ReadPointer(begin + index * 4, word))
	{
		return false;
	}

	if (!TryReadMemory(out, reinterpret_cast<const void*>(word), size - 1))
		return false;

	out[size - 1] = '\0';
	return out[0] != '\0';
}
