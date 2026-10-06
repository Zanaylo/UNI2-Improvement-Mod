#include "Core/HexText.h"

#include <cstring>

namespace {

constexpr char kDigits[] = "0123456789abcdef";
constexpr int kNoNibble = -1;

int NibbleOf(char digit)
{
	if (digit >= '0' && digit <= '9')
		return digit - '0';

	if (digit >= 'a' && digit <= 'f')
		return digit - 'a' + 10;

	return kNoNibble;
}

}

void HexText::Encode(const uint8_t* bytes, int count, char* out)
{
	for (int i = 0; i < count; ++i)
	{
		out[i * 2] = kDigits[bytes[i] >> 4];
		out[i * 2 + 1] = kDigits[bytes[i] & 0xf];
	}

	out[count * 2] = '\0';
}

bool HexText::Decode(const char* text, uint8_t* out, int count)
{
	if (text == nullptr || strlen(text) != static_cast<size_t>(count * 2))
		return false;

	for (int i = 0; i < count; ++i)
	{
		const int high = NibbleOf(text[i * 2]);
		const int low = NibbleOf(text[i * 2 + 1]);

		if (high == kNoNibble || low == kNoNibble)
			return false;

		out[i] = static_cast<uint8_t>(high << 4 | low);
	}

	return true;
}
