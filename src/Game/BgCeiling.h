#pragma once

#include <cstdint>

namespace BgCeiling
{
	constexpr int kStockNumbers = 100;
	constexpr int kWideNumbers = 500;
	constexpr int kWideListEntries = 500;

	bool Initialize();

	bool Lifted();
	bool Restacked();

	int Numbers();
	int ListEntries();

	uintptr_t RecordTable();

	const char* StatusText();
}
