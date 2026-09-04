#pragma once

#include <cstdint>
#include <vector>

namespace MbtlCipher
{
	void Decrypt(std::vector<uint8_t>& data);

	uint32_t Phase(const std::vector<uint8_t>& data);

	void DecryptAt(std::vector<uint8_t>& data, uint32_t phase);
}
