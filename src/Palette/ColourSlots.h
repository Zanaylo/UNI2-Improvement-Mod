#pragma once

namespace ColourSlots
{
	constexpr int kGameColours = 0x2f;
	constexpr int kStockColours = 0x2a;
	constexpr int kCharacters = 64;
	constexpr int kNoColour = -1;
	constexpr int kNoExtended = 0;

	const char* BoundFile(int chara, int colour);
	int ColourOf(int chara, const char* file);

	void Bind(int chara, int colour, const char* file);
	void Release(int chara, const char* file);

	int ExtendedCount(int chara);
	const char* ExtendedFile(int chara, int extended);
	int ExtendedOf(int chara, const char* file);

	void Reload();
}
