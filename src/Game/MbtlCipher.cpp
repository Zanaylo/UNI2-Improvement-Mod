#include "Game/MbtlCipher.h"

namespace {

#include "Game/OstMbtlKey.inc"

constexpr uint32_t kMask = 0x3ff;
constexpr uint32_t kSeed = 0x76381;

uint32_t Lead(std::vector<uint8_t>& data)
{
	data[0] ^= 0xa5;
	data[1] ^= 0x18;

	return static_cast<uint32_t>(data[0]) ^ 0xac;
}

}

void MbtlCipher::Decrypt(std::vector<uint8_t>& data)
{
	if (data.size() < 2)
		return;

	DecryptAt(data, Phase(data));
}

uint32_t MbtlCipher::Phase(const std::vector<uint8_t>& data)
{
	if (data.size() < 2)
		return 0;

	const uint32_t lead = (static_cast<uint32_t>(data[0]) ^ 0xa5) ^ 0xac;
	const uint32_t seed = lead ^ (static_cast<uint32_t>(data[1]) ^ 0x18) ^ kSeed;

	return (seed + static_cast<uint32_t>(data.size()) - 1) & kMask;
}

void MbtlCipher::DecryptAt(std::vector<uint8_t>& data, uint32_t phase)
{
	if (data.size() < 2)
		return;

	const uint32_t lead = Lead(data);

	for (size_t i = 2; i < data.size(); ++i)
		data[i] ^= kMbtlKey[lead ^ ((phase - static_cast<uint32_t>(i)) & kMask)];
}
