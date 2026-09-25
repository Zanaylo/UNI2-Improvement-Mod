#pragma once

namespace ColorCustomize
{
	constexpr int kSlotCount = 5;
	constexpr int kPartCount = 6;

	constexpr int kStockLimit = 42;
	constexpr int kCustomFirst = kStockLimit;
	constexpr int kColourLimit = kCustomFirst + kSlotCount;

	bool IsAvailable();

	const char* PartName(int part);

	bool GetEquipped(int chara, int& outColour);
	bool SetEquipped(int chara, int colour);

	bool GetSlot(int chara, int slot, int* outValues);
	bool SetSlot(int chara, int slot, const int* values);

	bool IsUnlocked(int chara, int colour);
	int CountLocked(int chara, int colours);
	bool Unlock(int chara, int colour);
}
