#include "Network/Spectate/SpectateCode.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr int kDigits = 13;
constexpr int kBitsPerDigit = 5;
constexpr uint64_t kDigitMask = 31;
constexpr uint64_t kAccountMask = 0xFFFFFFFF00000000ull;
constexpr uint64_t kIndividualAccount = 0x0110000100000000ull;

int ValueOf(char c)
{
	const char upper = static_cast<char>(toupper(static_cast<unsigned char>(c)));

	if (upper == 0)
		return -1;

	if (upper == 'O')
		return 0;

	if (upper == 'I' || upper == 'L')
		return 1;

	const char* const found = strchr(kAlphabet, upper);

	return found == nullptr ? -1 : static_cast<int>(found - kAlphabet);
}

bool Separator(char c)
{
	return c == '-' || c == ' ';
}

}

void SpectateCode::Encode(uint64_t steamId, char* out, int size)
{
	char digits[kDigits] = {};
	uint64_t rest = steamId;

	for (int i = kDigits - 1; i >= 0; --i)
	{
		digits[i] = kAlphabet[rest & kDigitMask];
		rest >>= kBitsPerDigit;
	}

	sprintf_s(out, size, "%.4s-%.4s-%.5s", digits, digits + 4, digits + 8);
}

bool SpectateCode::Decode(const char* text, uint64_t& out)
{
	out = 0;

	if (text == nullptr)
		return false;

	uint64_t value = 0;
	int digits = 0;

	for (const char* at = text; *at != 0; ++at)
	{
		if (Separator(*at))
			continue;

		const int digit = ValueOf(*at);

		if (digit < 0 || digits == kDigits)
			return false;

		value = (value << kBitsPerDigit) | static_cast<uint64_t>(digit);
		++digits;
	}

	if (digits != kDigits || (value & kAccountMask) != kIndividualAccount)
		return false;

	out = value;
	return true;
}
