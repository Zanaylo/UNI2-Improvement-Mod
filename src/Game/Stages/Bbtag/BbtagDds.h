#pragma once

#include <cstdint>
#include <vector>

namespace BbtagDds
{
	uint32_t FirstLevelBytes(const std::vector<uint8_t>& dds);

	void StateLinearSize(std::vector<uint8_t>& dds);
}
