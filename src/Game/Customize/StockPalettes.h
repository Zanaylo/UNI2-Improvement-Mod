#pragma once

#include <cstdint>

namespace StockPalettes
{
	constexpr int kColours = 256;

	bool Load(int chara);

	int GetCount(int chara);

	const uint8_t* GetRow(int chara, int palette);

	int GetStoredCount(int chara);

	const uint8_t* GetStoredRows(int chara);
}
