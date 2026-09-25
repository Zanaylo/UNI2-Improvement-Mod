#pragma once

#include <cstdint>
#include <vector>

namespace BgmTableFile
{
	bool HasVanillaSlots(const std::vector<uint8_t>& blob);

	bool ReadGameTable(std::vector<uint8_t>& out);

	bool Refresh(std::vector<uint8_t>& table);

	void Repair();
}
