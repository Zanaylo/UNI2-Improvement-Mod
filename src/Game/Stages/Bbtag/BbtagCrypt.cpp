#include "Game/Stages/Bbtag/BbtagCrypt.h"

#include <cstring>

namespace {

const uint8_t kKey[43] = {
	0xf5, 0x5c, 0x84, 0x2a, 0xad, 0x61, 0x54, 0xe7, 0x0a, 0xfc, 0x99, 0x6b, 0xd5, 0xa4, 0xd3, 0xd8,
	0x48, 0x26, 0x69, 0xcb, 0x07, 0x42, 0x13, 0x5e, 0x10, 0x23, 0xd2, 0x6d, 0x36, 0xc7, 0xc1, 0x66,
	0xdf, 0xa1, 0xad, 0xf1, 0x44, 0x44, 0x7e, 0xc9, 0x8e, 0x24, 0x99,
};

const uint32_t kSine[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

const int kShift[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

uint32_t Rotate(uint32_t value, int by)
{
	return (value << by) | (value >> (32 - by));
}

void Digest(const std::string& text, uint8_t out[16])
{
	std::vector<uint8_t> padded(text.begin(), text.end());
	const uint64_t bits = static_cast<uint64_t>(padded.size()) * 8;

	padded.push_back(0x80);

	while (padded.size() % 64 != 56)
		padded.push_back(0);

	for (int i = 0; i < 8; ++i)
		padded.push_back(static_cast<uint8_t>(bits >> (i * 8)));

	uint32_t state[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };

	for (size_t at = 0; at < padded.size(); at += 64)
	{
		uint32_t block[16] = {};

		for (int i = 0; i < 16; ++i)
		{
			block[i] = static_cast<uint32_t>(padded[at + i * 4])
				| (static_cast<uint32_t>(padded[at + i * 4 + 1]) << 8)
				| (static_cast<uint32_t>(padded[at + i * 4 + 2]) << 16)
				| (static_cast<uint32_t>(padded[at + i * 4 + 3]) << 24);
		}

		uint32_t a = state[0];
		uint32_t b = state[1];
		uint32_t c = state[2];
		uint32_t d = state[3];

		for (int i = 0; i < 64; ++i)
		{
			uint32_t mixed = 0;
			int taken = 0;

			if (i < 16)
			{
				mixed = (b & c) | (~b & d);
				taken = i;
			}
			else if (i < 32)
			{
				mixed = (d & b) | (~d & c);
				taken = (5 * i + 1) % 16;
			}
			else if (i < 48)
			{
				mixed = b ^ c ^ d;
				taken = (3 * i + 5) % 16;
			}
			else
			{
				mixed = c ^ (b | ~d);
				taken = (7 * i) % 16;
			}

			const uint32_t carried = d;

			d = c;
			c = b;
			b = b + Rotate(a + mixed + kSine[i] + block[taken], kShift[i]);
			a = carried;
		}

		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
	}

	for (int i = 0; i < 4; ++i)
	{
		for (int byte = 0; byte < 4; ++byte)
			out[i * 4 + byte] = static_cast<uint8_t>(state[i] >> (byte * 8));
	}
}

std::string Lowered(const std::string& text)
{
	std::string out = text;

	for (char& letter : out)
	{
		if (letter >= 'A' && letter <= 'Z')
			letter = static_cast<char>(letter - 'A' + 'a');
	}

	return out;
}

}

std::string BbtagCrypt::Md5(const std::string& text)
{
	uint8_t digest[16] = {};
	Digest(Lowered(text), digest);

	static const char* const hex = "0123456789abcdef";
	std::string out;

	for (uint8_t value : digest)
	{
		out.push_back(hex[value >> 4]);
		out.push_back(hex[value & 15]);
	}

	return out;
}

std::string BbtagCrypt::NameOf(const std::string& relative)
{
	std::string path = Lowered(relative);

	for (char& letter : path)
	{
		if (letter == '\\')
			letter = '/';
	}

	return Md5(path);
}

void BbtagCrypt::Decrypt(const std::string& name, std::vector<uint8_t>& data)
{
	uint8_t digest[16] = {};
	Digest(Lowered(name), digest);

	size_t index = digest[7] % sizeof(kKey);

	for (uint8_t& value : data)
	{
		value ^= kKey[index];
		index = (index + 1) % sizeof(kKey);
	}
}
