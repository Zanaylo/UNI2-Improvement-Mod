#include "Game/HitboxData.h"

#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/MemoryMap.h"

#include <Windows.h>
#include <cstdlib>

namespace {

constexpr int kCoordinateLimit = 30000;

bool LooksLikePointer(uint32_t value)
{
	return value >= 0x10000 && (value & 3) == 0;
}

bool ReadRecord(uint32_t record, HitboxData::Box& out)
{
	int16_t coords[4] = {};
	if (!TryReadMemory(coords, reinterpret_cast<const void*>(record), sizeof(coords)))
		return false;

	out.x1 = coords[0];
	out.y1 = coords[1];
	out.x2 = coords[2];
	out.y2 = coords[3];

	if (out.x1 == out.x2 || out.y1 == out.y2)
		return false;

	if (abs(out.x1) > kCoordinateLimit || abs(out.y1) > kCoordinateLimit ||
		abs(out.x2) > kCoordinateLimit || abs(out.y2) > kCoordinateLimit)
	{
		return false;
	}

	return true;
}

int CountValidBoxes(uint32_t arrayPointer, int count)
{
	if (count <= 0 || count > HitboxData::kMaxBoxesPerArray || !LooksLikePointer(arrayPointer))
		return 0;

	uint32_t entries[HitboxData::kMaxBoxesPerArray] = {};
	if (!TryReadMemory(entries, reinterpret_cast<const void*>(arrayPointer),
		sizeof(uint32_t) * count))
	{
		return 0;
	}

	int valid = 0;
	for (int i = 0; i < count; ++i)
	{
		if (!LooksLikePointer(entries[i]))
			continue;

		HitboxData::Box box = {};
		if (ReadRecord(entries[i], box))
			++valid;
	}

	return valid;
}

int HanteiCategory(int arrayIndex, int index)
{
	if (arrayIndex == HitboxData::BoxArray_Attack)
		return 3;

	if (index == 0)
		return 0;

	if (index <= 8)
		return 1;

	return 2;
}

bool CategorySuppressed(uint32_t existFlags, int category)
{
	switch (category)
	{
	case 0: return (existFlags & GameOffsets::kExistNoKasanariHantei) != 0;
	case 1: return (existFlags & GameOffsets::kExistNoKuraiHantei) != 0;
	case 2: return (existFlags & GameOffsets::kExistNoEtcHantei) != 0;
	case 3: return (existFlags & GameOffsets::kExistNoAttackHantei) != 0;
	default: return false;
	}
}

bool ReadFrameObjectAt(void* entity, uintptr_t offset, HitboxData::FrameObject& out)
{
	uint32_t candidate = 0;
	if (!MemoryMap::ReadStructDword(entity, offset, candidate))
		return false;

	if (!LooksLikePointer(candidate))
		return false;

	uint8_t counts[HitboxData::kArrayCount] = {};
	if (!TryReadMemory(counts, reinterpret_cast<const void*>(candidate + GameOffsets::kFrameObjectCounts),
		sizeof(counts)))
	{
		return false;
	}

	uint32_t arrays[HitboxData::kArrayCount] = {};
	if (!TryReadMemory(arrays, reinterpret_cast<const void*>(candidate + GameOffsets::kFrameObjectArrays),
		sizeof(arrays)))
	{
		return false;
	}

	for (int i = 0; i < HitboxData::kArrayCount; ++i)
	{
		if (counts[i] > HitboxData::kMaxBoxesPerArray)
			return false;

		if (counts[i] > 0 && !LooksLikePointer(arrays[i]))
			return false;
	}

	out.pointer = reinterpret_cast<void*>(candidate);
	out.offsetInPlayerData = offset;
	for (int i = 0; i < HitboxData::kArrayCount; ++i)
	{
		out.arrays[i] = reinterpret_cast<void*>(arrays[i]);
		out.counts[i] = counts[i];
	}

	MemoryMap::ReadStructDword(entity, GameOffsets::kCharaExistFlags, out.existFlags);

	return true;
}

}

bool HitboxData::Resolve(void* playerData, FrameObject& out)
{
	memset(&out, 0, sizeof(out));

	if (MemoryMap::ClassifyEntity(playerData) == MemoryMap::EntityKind::Unknown)
		return false;

	return ReadFrameObjectAt(playerData, GameOffsets::kCharaFrameObject, out);
}

HitboxData::ScanResult HitboxData::ScanForFrameObject(void* playerData)
{
	ScanResult result = {};

	if (MemoryMap::ClassifyEntity(playerData) == MemoryMap::EntityKind::Unknown)
		return result;

	for (uintptr_t offset = 4; offset + 4 <= GameOffsets::kPlayerDataSize; offset += 4)
	{
		FrameObject current = {};
		if (!ReadFrameObjectAt(playerData, offset, current))
			continue;

		int score = 0;
		for (int i = 0; i < kArrayCount; ++i)
			score += CountValidBoxes(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(current.arrays[i])),
				current.counts[i]);

		if (score <= 0)
			continue;

		++result.candidates;

		if (score > result.bestScore)
		{
			result.found = true;
			result.bestScore = score;
			result.offset = offset;
		}
	}

	return result;
}

int HitboxData::ReadBoxes(const FrameObject& frameObject, Box* outBoxes, int maxBoxes)
{
	if (outBoxes == nullptr || maxBoxes <= 0)
		return 0;

	int count = 0;

	for (int arrayIndex = kFirstBoxArray; arrayIndex < kArrayCount && count < maxBoxes; ++arrayIndex)
	{
		const int entryCount = frameObject.counts[arrayIndex];
		const uint32_t arrayPointer =
			static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frameObject.arrays[arrayIndex]));

		if (entryCount <= 0 || entryCount > kMaxBoxesPerArray || !LooksLikePointer(arrayPointer))
			continue;

		uint32_t entries[kMaxBoxesPerArray] = {};
		if (!TryReadMemory(entries, reinterpret_cast<const void*>(arrayPointer),
			sizeof(uint32_t) * entryCount))
		{
			continue;
		}

		for (int i = 0; i < entryCount && count < maxBoxes; ++i)
		{
			if (!LooksLikePointer(entries[i]))
				continue;

			Box box = {};
			if (!ReadRecord(entries[i], box))
				continue;

			box.index = i;
			box.arrayIndex = arrayIndex;

			if (CategorySuppressed(frameObject.existFlags, HanteiCategory(arrayIndex, i)))
				continue;

			outBoxes[count++] = box;
		}
	}

	return count;
}
