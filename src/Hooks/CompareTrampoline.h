#pragma once

#include <cstddef>
#include <cstdint>

namespace CompareTrampoline
{
	constexpr size_t kBytes = 20;
	constexpr size_t kLongestSite = 9;

	struct Site
	{
		uint8_t* at;
		uint8_t modrm;
		uint8_t condition;
		uint8_t counted;
		size_t jumpBytes;
		uint8_t* target;
	};

	bool Decode(uint8_t* at, Site& out);

	bool Emit(const Site& site, uint8_t* trampoline, int32_t bound);
}
