#include "Palette/EffectBlock.h"

#include "Game/Tables/PartColourTable.h"

#include <cstring>

namespace {

constexpr int kEntries = 256;
constexpr int kBytes = kEntries * 4;
constexpr uint8_t kMarked = 255;

bool IsMarked(const uint8_t* block, int entry)
{
	return block[entry * 4 + 3] == kMarked;
}

bool Overlay(const uint8_t* page, uint8_t* out)
{
	bool any = false;

	for (int entry = 1; entry < kEntries; ++entry)
	{
		if (!IsMarked(page, entry))
			continue;

		memcpy(out + entry * 4, page + entry * 4, 4);
		any = true;
	}

	return any;
}

}

bool EffectBlock::Compose(int chara, const uint8_t* colours, const uint8_t* page, uint8_t* out)
{
	memset(out, 0, kBytes);

	const bool parts = PartColourTable::BuildAutoEffectBlock(chara, colours, out);
	const bool marked = page != nullptr && Overlay(page, out);

	return parts || marked;
}
