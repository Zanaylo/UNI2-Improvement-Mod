#include "Game/Engine/PlayerHealth.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"

#include <cstdint>

namespace {

constexpr int kValidatedRecordSlots = 4;

struct ValidatedRecord
{
	uint32_t record;
	bool valid;
};

ValidatedRecord g_validatedRecords[kValidatedRecordSlots] = {};
int g_nextValidatedSlot = 0;

bool IsRecordAlreadyValidated(uint32_t record)
{
	for (const ValidatedRecord& entry : g_validatedRecords)
	{
		if (entry.valid && entry.record == record)
			return true;
	}

	return false;
}

void RememberValidatedRecord(uint32_t record)
{
	g_validatedRecords[g_nextValidatedSlot] = { record, true };
	g_nextValidatedSlot = (g_nextValidatedSlot + 1) % kValidatedRecordSlots;
}

bool ReadHealthFields(void* playerData, int32_t& outCurrent, int32_t& outTrailing, uint32_t& outRecord)
{
	const uintptr_t base = reinterpret_cast<uintptr_t>(playerData);
	if (base < 0x10000)
		return false;

	bool ok = false;

	__try
	{
		outCurrent = *reinterpret_cast<const int32_t*>(base + GameOffsets::kPlayerDataHp);
		outTrailing = *reinterpret_cast<const int32_t*>(base + GameOffsets::kPlayerDataHpTrailing);
		outRecord = *reinterpret_cast<const uint32_t*>(base + GameOffsets::kPlayerDataHpRecord);
		ok = true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ok = false;
	}

	return ok;
}

bool ReadMaxFromRecord(uint32_t record, int& out)
{
	if (record == 0)
		return false;

	if (!IsRecordAlreadyValidated(record))
	{
		if (!IsReadableMemory(reinterpret_cast<const void*>(record),
			GameOffsets::kHpRecordFirstSegment + GameOffsets::kHpRecordSegments * sizeof(int32_t)))
		{
			return false;
		}

		RememberValidatedRecord(record);
	}

	int32_t values[GameOffsets::kHpRecordSegments] = {};
	const uint8_t* const segments =
		reinterpret_cast<const uint8_t*>(record) + GameOffsets::kHpRecordFirstSegment;

	if (!TryReadMemory(values, segments, sizeof(values)))
		return false;

	int total = 0;
	for (int segment = 0; segment < GameOffsets::kHpRecordSegments; ++segment)
		total += values[segment];

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
	uint32_t record = 0;
	int max = 0;

	if (!ReadHealthFields(playerData, current, trailing, record))
		return false;

	if (!ReadMaxFromRecord(record, max))
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
