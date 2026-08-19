#include "Game/ColorCustomize.h"

#include "Core/utils.h"
#include "Game/CharaTables.h"
#include "Game/GameOffsets.h"
#include "Game/PlayerCard.h"

#include <cstdint>

namespace {

constexpr const char* kPartNames[ColorCustomize::kPartCount] = {
	"Base",
	"Part 1",
	"Part 2",
	"Part 3",
	"Part 4",
	"Part 5",
};

void* Address(uintptr_t rva)
{
	const uintptr_t address = RvaToAddress(rva);
	if (address == 0)
		return nullptr;

	return reinterpret_cast<void*>(address);
}

bool ValidCharacter(int chara)
{
	return chara >= 0 && chara < CharaTables::GetCharaCount()
		&& chara < GameOffsets::kColourCharacterMax;
}

bool ValidSlot(int slot)
{
	return slot >= 0 && slot < ColorCustomize::kSlotCount;
}

bool ValidEntry(int value)
{
	return value >= 0 && value < ColorCustomize::kColourLimit;
}

int EntryIndex(int chara, int slot, int part)
{
	return chara * GameOffsets::kColourSlotsPerCharacter * GameOffsets::kColourEntriesPerSlot
		+ slot * GameOffsets::kColourEntriesPerSlot + part;
}

bool ReadPacked(int index, uint8_t& outValue)
{
	const void* const source = Address(GameOffsets::kColourSlots + index);
	if (source == nullptr)
		return false;

	return TryReadMemory(&outValue, source, sizeof(outValue));
}

bool WritePacked(int index, uint8_t value)
{
	void* const target = Address(GameOffsets::kColourSlots + index);
	if (target == nullptr)
		return false;

	return TryWriteMemory(target, &value, sizeof(value));
}

bool WriteLive(int index, uint8_t value)
{
	void* const target = Address(GameOffsets::kColourSlotsLive + index * sizeof(uint32_t));
	if (target == nullptr)
		return false;

	const uint32_t widened = value;
	return TryWriteMemory(target, &widened, sizeof(widened));
}

bool ReadUnlockBits(int chara, uint32_t& outBits)
{
	const void* const source =
		Address(GameOffsets::kColourUnlockBits + chara * sizeof(uint32_t));
	if (source == nullptr)
		return false;

	return TryReadMemory(&outBits, source, sizeof(outBits));
}

bool WriteUnlockBits(int chara, uint32_t bits)
{
	void* const target = Address(GameOffsets::kColourUnlockBits + chara * sizeof(uint32_t));
	if (target == nullptr)
		return false;

	return TryWriteMemory(target, &bits, sizeof(bits));
}

bool WriteUnlockEntry(int chara, int colour, uint8_t value)
{
	if (chara >= GameOffsets::kColourUnlockCharacterMax)
		return true;

	void* const target = Address(GameOffsets::kColourUnlockTable
		+ chara * GameOffsets::kColourUnlockStride + colour);
	if (target == nullptr)
		return false;

	return TryWriteMemory(target, &value, sizeof(value));
}

int UnlockBit(int colour)
{
	return colour - GameOffsets::kColourFreeBelow;
}

bool NeedsUnlock(int colour)
{
	if (colour >= ColorCustomize::kCustomFirst)
		return false;

	const int bit = UnlockBit(colour);
	return bit >= 0 && bit < GameOffsets::kColourUnlockBitCount;
}

}

bool ColorCustomize::IsAvailable()
{
	if (!PlayerCard::IsAvailable())
		return false;

	const void* const live = Address(GameOffsets::kColourSlotsLive);
	if (live == nullptr)
		return false;

	constexpr size_t kLiveBytes = GameOffsets::kColourCharacterMax
		* GameOffsets::kColourSlotsPerCharacter * GameOffsets::kColourEntriesPerSlot
		* sizeof(uint32_t);

	return IsReadableMemory(live, kLiveBytes);
}

const char* ColorCustomize::PartName(int part)
{
	if (part < 0 || part >= kPartCount)
		return "";

	return kPartNames[part];
}

bool ColorCustomize::GetEquipped(int chara, int& outColour)
{
	if (!ValidCharacter(chara))
		return false;

	const void* const source = Address(GameOffsets::kColourEquipped + chara);
	if (source == nullptr)
		return false;

	uint8_t value = 0;
	if (!TryReadMemory(&value, source, sizeof(value)))
		return false;

	outColour = value;
	return true;
}

bool ColorCustomize::SetEquipped(int chara, int colour)
{
	if (!ValidCharacter(chara) || !ValidEntry(colour))
		return false;

	if (!Unlock(chara, colour))
		return false;

	void* const target = Address(GameOffsets::kColourEquipped + chara);
	if (target == nullptr)
		return false;

	const uint8_t value = static_cast<uint8_t>(colour);
	if (!TryWriteMemory(target, &value, sizeof(value)))
		return false;

	PlayerCard::MarkChanged();
	return true;
}

bool ColorCustomize::GetSlot(int chara, int slot, int* outValues)
{
	if (!ValidCharacter(chara) || !ValidSlot(slot) || outValues == nullptr)
		return false;

	for (int part = 0; part < kPartCount; ++part)
	{
		uint8_t value = 0;
		if (!ReadPacked(EntryIndex(chara, slot, part), value))
			return false;

		outValues[part] = value;
	}

	return true;
}

bool ColorCustomize::SetSlot(int chara, int slot, const int* values)
{
	if (!ValidCharacter(chara) || !ValidSlot(slot) || values == nullptr)
		return false;

	for (int part = 0; part < kPartCount; ++part)
	{
		if (!ValidEntry(values[part]))
			return false;
	}

	for (int part = 0; part < kPartCount; ++part)
	{
		const int colour = values[part];
		if (!Unlock(chara, colour))
			return false;

		const int index = EntryIndex(chara, slot, part);
		const uint8_t packed = static_cast<uint8_t>(colour);

		if (!WritePacked(index, packed) || !WriteLive(index, packed))
			return false;
	}

	PlayerCard::MarkChanged();
	return true;
}

bool ColorCustomize::IsUnlocked(int chara, int colour)
{
	if (!ValidCharacter(chara) || !ValidEntry(colour))
		return false;

	if (!NeedsUnlock(colour))
		return true;

	uint32_t bits = 0;
	if (!ReadUnlockBits(chara, bits))
		return false;

	return (bits & (1u << UnlockBit(colour))) != 0;
}

int ColorCustomize::CountLocked(int chara, int colours)
{
	const int limit = colours < kStockLimit ? colours : kStockLimit;

	int locked = 0;

	for (int colour = 0; colour < limit; ++colour)
	{
		if (NeedsUnlock(colour) && !IsUnlocked(chara, colour))
			++locked;
	}

	return locked;
}

bool ColorCustomize::Unlock(int chara, int colour)
{
	if (!ValidCharacter(chara) || !ValidEntry(colour))
		return false;

	if (!NeedsUnlock(colour))
		return true;

	uint32_t bits = 0;
	if (!ReadUnlockBits(chara, bits))
		return false;

	const uint32_t mask = 1u << UnlockBit(colour);
	if ((bits & mask) != 0)
		return true;

	if (!WriteUnlockBits(chara, bits | mask) || !WriteUnlockEntry(chara, colour, 1))
		return false;

	PlayerCard::MarkChanged();
	return true;
}
