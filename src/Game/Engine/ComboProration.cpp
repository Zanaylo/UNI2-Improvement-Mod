#include "Game/Engine/ComboProration.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/MemoryMap.h"

#include <algorithm>
#include <cstdint>

namespace {

constexpr int kMaxEntries = 32;

struct RateEntry
{
	int32_t upTo;
	int32_t rate;
};

bool ReadInt(uintptr_t address, int& out)
{
	uint32_t value = 0;

	if (!MemoryMap::ReadDwordAt(address, value))
		return false;

	out = static_cast<int>(value);
	return true;
}

int Lookup(uintptr_t status, uintptr_t table, int key)
{
	uint32_t begin = 0;
	uint32_t end = 0;

	if (!MemoryMap::ReadDwordAt(status + table, begin) || !MemoryMap::ReadDwordAt(status + table + 4, end))
		return ComboProration::kFullRate;

	const int count = (std::min)(static_cast<int>((end - begin) / GameOffsets::kRateTableEntrySize), kMaxEntries);

	if (begin == 0 || end < begin || count <= 0)
		return ComboProration::kFullRate;

	RateEntry entry = {};

	for (int i = 0; i < count - 1; ++i)
	{
		if (!TryReadMemory(&entry, reinterpret_cast<const void*>(begin + i * GameOffsets::kRateTableEntrySize),
			sizeof(entry)))
			return ComboProration::kFullRate;

		if (key > entry.upTo)
			continue;

		if (entry.rate >= 0)
			return entry.rate;

		break;
	}

	if (!TryReadMemory(&entry,
		reinterpret_cast<const void*>(begin + (count - 1) * GameOffsets::kRateTableEntrySize), sizeof(entry)))
		return ComboProration::kFullRate;

	return (std::max)(entry.rate, 0);
}

}

bool ComboProration::Read(int player, Reading& out)
{
	out = Reading();

	const uintptr_t chara = reinterpret_cast<uintptr_t>(MemoryMap::GetCharaSlot(player));

	if (chara == 0)
		return false;

	uint8_t side = 0;

	if (!TryReadMemory(&side, reinterpret_cast<const void*>(chara + GameOffsets::kCharaSideIndex), sizeof(side)) ||
		side > 1)
		return false;

	const uintptr_t record = RvaToAddress(GameOffsets::kComboRecordBase) + side * GameOffsets::kComboRecordStride;

	int valid = 0;
	uint32_t status = 0;

	if (!ReadInt(record + GameOffsets::kComboRecordValid, valid) ||
		!ReadInt(record + GameOffsets::kComboTimer, out.timer) ||
		!ReadInt(record + GameOffsets::kComboMoveCount, out.moves) ||
		!ReadInt(record + GameOffsets::kComboHitCount, out.hits) ||
		!ReadInt(record + GameOffsets::kComboDamageTotal, out.damage) ||
		!MemoryMap::ReadDwordAt(chara + GameOffsets::kCharaStatusTable, status) || status == 0)
		return false;

	out.active = valid != 0;

	if (!out.active)
		out.timer = 0;

	out.timeRate = Lookup(status, GameOffsets::kStatusAttackRateTime, out.timer);
	out.moveRate = out.moves > 0 ? Lookup(status, GameOffsets::kStatusAttackRateMoveCount, out.moves) : kFullRate;

	return true;
}
