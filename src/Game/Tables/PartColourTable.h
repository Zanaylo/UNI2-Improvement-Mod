#pragma once

#include <cstdint>

namespace PartColourTable
{
	bool IsPartEntry(int chara, int entry);
	int GetPartCount(int chara, int entry);

	bool BuildAutoEffectBlock(int chara, const uint8_t* colours, uint8_t* outEffect);
}
