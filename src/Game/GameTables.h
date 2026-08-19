// The game's own names for the numbers this project reads, out of its unpacked data files.

#pragma once

#include <cstdint>

namespace GameTables
{

	const char* CommandName(uint32_t command);

	const char* PatternName(uint16_t pattern);

	int GetMoveCodeBitCount();
	bool GetMoveCodeBit(int index, uint32_t& outMask, const char*& outName);

	const char* PaletteName(int chara, int palette);
	int GetPaletteNameCount();

	int GetPartCount(int chara);
	bool GetPart(int chara, int index, const char*& outName, const unsigned char*& outEntries,
		int& outCount);
}
