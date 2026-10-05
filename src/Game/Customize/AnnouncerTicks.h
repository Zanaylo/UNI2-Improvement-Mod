#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace AnnouncerTicks
{
	constexpr int kSlots = 64;

	using Flags = std::array<uint8_t, kSlots>;
	using Slots = std::array<bool, kSlots>;

	Slots Imported(const std::vector<int>& stockIds);
	void Keep(const Slots& imported, const Flags& before, uint8_t* ticked, uint8_t* excluded);
}
