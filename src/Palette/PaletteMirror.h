#pragma once

#include <cstdint>

namespace PaletteMirror
{
	constexpr unsigned kMirroredRowBit = 1;

	constexpr bool IsMirroredRow(unsigned row)
	{
		return (row & kMirroredRowBit) != 0;
	}

	const uint8_t* FacingLeft(int chara, const uint8_t* rgba, uint8_t* buffer);

	void Unmirror(int chara, uint8_t* rgba);
}
