// The game's own colour customiser: five slots per character, each one base palette plus five part
// palettes, packed one byte per entry inside the player card block.
//
// This is the half of the palette story that travels. The block is copied verbatim into the
// opponent's player-slot record and the opponent's game composes the colours out of its own files,
// so a slot written here is seen by every player, modded or not. What it cannot do is name an
// arbitrary colour: an entry is an index into the character's own stock palettes and nothing else.
// docs/CUSTOMIZE.md 7b has the layout and how it was settled.

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
