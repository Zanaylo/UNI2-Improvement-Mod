#pragma once

#include <cstdint>

namespace EffectOwner
{
	constexpr int kPlayers = 2;
	constexpr int kEntries = 256;

	enum class Route
	{
		None,
		Worn,
		Stock,
		Claim,
		Ambiguous,
		SoleWanter,
	};

	void OnFrame();

	int PlayerFor(int entry, uint8_t r, uint8_t g, uint8_t b, Route& outRoute);

	bool Claims(int player, int entry);

	bool IsMirror();

	bool GetWorn(int player, int entry, uint8_t* outRgb);
	int GetStockCount(int player, int entry);
	int FindStock(int player, int entry, uint8_t r, uint8_t g, uint8_t b);

	void GetCounts(int& outWorn, int& outStock, int& outClaim, int& outAmbiguous,
		int& outUnresolved);
	void NoteSoleWanter();
	int GetSoleWanters();
	void ResetCounts();

	const char* Describe();
}
