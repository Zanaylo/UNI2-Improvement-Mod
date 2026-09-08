#include "Game/UnielCipher.h"

#include <utility>

namespace {

constexpr uint8_t kKey[] = {
	0xd3, 0x04, 0xf5, 0x27, 0xf3, 0x2e, 0x29, 0x9c,
	0x96, 0xf6, 0xfe, 0x4f, 0x47, 0xdd, 0xf4, 0xa9,
};

constexpr int kStateSize = 256;

}

void UnielCipher::Decrypt(std::vector<uint8_t>& data)
{
	if (data.empty())
		return;

	uint8_t state[kStateSize];

	for (int i = 0; i < kStateSize; ++i)
		state[i] = static_cast<uint8_t>(i);

	uint8_t mix = 0;

	for (int i = 0; i < kStateSize; ++i)
	{
		mix = static_cast<uint8_t>(mix + state[i] + kKey[i % sizeof(kKey)]);
		std::swap(state[i], state[mix]);
	}

	uint8_t step = 0;
	mix = 0;

	for (uint8_t& byte : data)
	{
		step = static_cast<uint8_t>(step + 1);
		mix = static_cast<uint8_t>(mix + state[step]);
		std::swap(state[step], state[mix]);
		byte ^= state[static_cast<uint8_t>(state[step] + state[mix])];
	}
}
