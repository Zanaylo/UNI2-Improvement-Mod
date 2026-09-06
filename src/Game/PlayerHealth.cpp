#include "Game/PlayerHealth.h"

#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <cstdint>

namespace {

const uint8_t* Field(void* playerData, uintptr_t offset)
{
	return static_cast<const uint8_t*>(playerData) + offset;
}

bool ReadMax(void* playerData, int& out)
{
	uint32_t record = 0;

	if (!TryReadMemory(&record, Field(playerData, GameOffsets::kPlayerDataHpRecord),
		sizeof(record)))
	{
		return false;
	}

	if (record == 0 || !IsReadableMemory(reinterpret_cast<const void*>(record),
		GameOffsets::kHpRecordFirstSegment + GameOffsets::kHpRecordSegments * sizeof(int32_t)))
	{
		return false;
	}

	const uint8_t* const segments =
		reinterpret_cast<const uint8_t*>(record) + GameOffsets::kHpRecordFirstSegment;

	int total = 0;

	for (int segment = 0; segment < GameOffsets::kHpRecordSegments; ++segment)
	{
		int32_t value = 0;

		if (!TryReadMemory(&value, segments + segment * sizeof(value), sizeof(value)))
			return false;

		total += value;
	}

	if (total <= 0)
		return false;

	out = total;
	return true;
}

}

bool PlayerHealth::Read(void* playerData, Health& out)
{
	if (playerData == nullptr)
		return false;

	int32_t current = 0;
	int32_t trailing = 0;
	int max = 0;

	if (!TryReadMemory(&current, Field(playerData, GameOffsets::kPlayerDataHp), sizeof(current)))
		return false;

	if (!TryReadMemory(&trailing, Field(playerData, GameOffsets::kPlayerDataHpTrailing),
		sizeof(trailing)))
	{
		return false;
	}

	if (!ReadMax(playerData, max))
		return false;

	if (current < 0)
		current = 0;

	if (trailing < current)
		trailing = current;

	if (current > max || trailing > max)
		return false;

	out.current = current;
	out.trailing = trailing;
	out.max = max;
	return true;
}
