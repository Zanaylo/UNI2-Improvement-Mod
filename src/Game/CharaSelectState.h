// The character-select scene keeps both sides in one heap block. The pointer to it is at RVA
// 0x754870 - every function the __FILE__ map places in scene\charaselect\*.cpp reaches through it -
// and the two sides are 0xc2c apart, measured from the preload thread at 0x24a050, which walks
// exactly two of them and hands one field to the './data/_csel/%s' palette lookup.
//
// Which field carries the highlighted character is theme data, not a constant here, so a wrong
// guess is corrected in screen.ini without a rebuild. Scan() names the candidates from a run.

#pragma once

#include <cstdint>

namespace CharaSelectState
{
	constexpr int kSideCount = 2;
	constexpr int kNoCharacter = -1;

	struct Layout
	{
		uintptr_t pointer;
		uint32_t stride;
		uint32_t field;
	};

	void Describe(const Layout& layout);

	bool IsLive();
	int CharacterOf(int side);

	bool ReadBlock(int side, uint32_t* out, int count);

	const char* StatusText();
}
