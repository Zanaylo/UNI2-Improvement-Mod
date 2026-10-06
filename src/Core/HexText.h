#pragma once

#include <cstdint>

namespace HexText
{
	void Encode(const uint8_t* bytes, int count, char* out);
	bool Decode(const char* text, uint8_t* out, int count);
}
