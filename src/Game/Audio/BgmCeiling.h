#pragma once

#include <cstdint>

namespace BgmCeiling
{
	constexpr int kStockSlots = 200;
	constexpr int kWideSlots = 1000;

	bool Initialize();

	bool Lifted();
	int Slots();
	uintptr_t Table();

	const char* StatusText();
}
