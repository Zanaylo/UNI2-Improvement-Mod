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
