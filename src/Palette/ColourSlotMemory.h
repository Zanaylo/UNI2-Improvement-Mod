#pragma once

namespace ColourSlotMemory
{
	constexpr int kSides = 2;

	void Remember(int side, int chara, const char* file);
	const char* Recall(int side, int chara);
}
