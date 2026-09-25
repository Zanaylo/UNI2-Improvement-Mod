#pragma once

#include <cstdint>

namespace SpectateCode
{
	constexpr int kTextBytes = 20;

	void Encode(uint64_t steamId, char* out, int size);
	bool Decode(const char* text, uint64_t& out);
}
