#include "Game/Menus/MenuKeys.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"

#include <cstdint>

int MenuKeys::KeyboardKey(int function)
{
	if (function < 0 || function >= GameOffsets::kMenuKeyCount)
		return kUnbound;

	uint8_t key = 0;
	const void* const source = reinterpret_cast<const void*>(
		RvaToAddress(GameOffsets::kKeyboardMenuKeys) + function);

	return TryReadMemory(&key, source, sizeof(key)) ? key : kUnbound;
}
