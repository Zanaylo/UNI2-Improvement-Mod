#include "Game/Menus/ColourNameTable.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Palette/ColourSlots.h"

namespace {

constexpr int kNoTable = -1;

}

uint8_t* ColourNameTable::Entry(int chara, int colour)
{
	if (colour < 0 || colour >= ColourSlots::kStockColours)
		return nullptr;

	const int* const index = reinterpret_cast<const int*>(
		RvaToAddress(GameOffsets::kColourNameTableIndex) + static_cast<uintptr_t>(chara) * sizeof(int));

	if (*index <= kNoTable)
		return nullptr;

	return reinterpret_cast<uint8_t*>(RvaToAddress(GameOffsets::kColourNameTable) +
		static_cast<uintptr_t>(*index) * GameOffsets::kColourNameCharaStride +
		static_cast<uintptr_t>(colour) * GameOffsets::kColourNameStride);
}

uint8_t* ColourNameTable::Swatch(uint8_t* entry, int swatch)
{
	return entry + GameOffsets::kColourNameSwatch + swatch * GameOffsets::kColourNameSwatchStride;
}
