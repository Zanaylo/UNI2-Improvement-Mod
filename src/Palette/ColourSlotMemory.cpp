#include "Palette/ColourSlotMemory.h"

#include "Palette/ColourSlots.h"

#include <cstring>

namespace {

constexpr int kFileLength = 128;

char g_files[ColourSlotMemory::kSides][ColourSlots::kCharacters][kFileLength] = {};

char* Find(int side, int chara)
{
	if (side < 0 || side >= ColourSlotMemory::kSides || chara < 0 || chara >= ColourSlots::kCharacters)
		return nullptr;

	return g_files[side][chara];
}

}

void ColourSlotMemory::Remember(int side, int chara, const char* file)
{
	char* const slot = Find(side, chara);

	if (slot == nullptr)
		return;

	strncpy_s(slot, kFileLength, file != nullptr ? file : "", _TRUNCATE);
}

const char* ColourSlotMemory::Recall(int side, int chara)
{
	const char* const slot = Find(side, chara);

	return slot != nullptr ? slot : "";
}
