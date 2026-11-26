#include "Game/Engine/HitboxData.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/MemoryMap.h"

#include <Windows.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

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

constexpr int kFrameCacheSlots = 32;

struct FrameCacheEntry
{
	void* entity;
	bool valid;
	HitboxData::FrameObject frame;
	HitboxData::Box boxes[HitboxData::kMaxBoxes];
	int count;
};

FrameCacheEntry g_frameCache[kFrameCacheSlots] = {};
int g_nextFrameCacheSlot = 0;

FrameCacheEntry* FindFrameCacheEntry(void* entity)
{
	for (FrameCacheEntry& slot : g_frameCache)
	{
		if (slot.valid && slot.entity == entity)
			return &slot;
	}

	return nullptr;
}

FrameCacheEntry& AllocateFrameCacheEntry(void* entity)
{
	FrameCacheEntry& slot = g_frameCache[g_nextFrameCacheSlot];
	g_nextFrameCacheSlot = (g_nextFrameCacheSlot + 1) % kFrameCacheSlots;

	slot.entity = entity;
	slot.valid = true;
	return slot;
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
	if (entity == nullptr)
		return false;

	const uintptr_t base = reinterpret_cast<uintptr_t>(entity);
	if (base < 0x10000 || (base & 3) != 0)
		return false;

	bool ok = false;
	uint32_t candidate = 0;
	uint8_t counts[HitboxData::kArrayCount] = {};
	uint32_t arrays[HitboxData::kArrayCount] = {};
	uint32_t existFlags = 0;

	__try
	{
		candidate = *reinterpret_cast<const uint32_t*>(base + offset);

		if (LooksLikePointer(candidate))
		{
			memcpy(counts, reinterpret_cast<const void*>(candidate + GameOffsets::kFrameObjectCounts),
				sizeof(counts));
			memcpy(arrays, reinterpret_cast<const void*>(candidate + GameOffsets::kFrameObjectArrays),
				sizeof(arrays));
			existFlags = *reinterpret_cast<const uint32_t*>(base + GameOffsets::kCharaExistFlags);
			ok = true;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ok = false;
	}

	if (!ok)
		return false;

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

	out.existFlags = existFlags;
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

	__try
	{
		for (int arrayIndex = kFirstBoxArray; arrayIndex < kArrayCount && count < maxBoxes; ++arrayIndex)
		{
			const int entryCount = frameObject.counts[arrayIndex];
			const uint32_t arrayPointer =
				static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frameObject.arrays[arrayIndex]));

			if (entryCount <= 0 || entryCount > kMaxBoxesPerArray || !LooksLikePointer(arrayPointer))
				continue;

			const uint32_t* const entries = reinterpret_cast<const uint32_t*>(arrayPointer);

			for (int i = 0; i < entryCount && count < maxBoxes; ++i)
			{
				const uint32_t record = entries[i];
				if (!LooksLikePointer(record))
					continue;

				const int16_t* const coords = reinterpret_cast<const int16_t*>(record);
				const int16_t x1 = coords[0];
				const int16_t y1 = coords[1];
				const int16_t x2 = coords[2];
				const int16_t y2 = coords[3];

				if (x1 == x2 || y1 == y2)
					continue;

				if (abs(x1) > kCoordinateLimit || abs(y1) > kCoordinateLimit ||
					abs(x2) > kCoordinateLimit || abs(y2) > kCoordinateLimit)
				{
					continue;
				}

				if (CategorySuppressed(frameObject.existFlags, HanteiCategory(arrayIndex, i)))
					continue;

				Box& box = outBoxes[count++];
				box.x1 = x1;
				box.y1 = y1;
				box.x2 = x2;
				box.y2 = y2;
				box.index = i;
				box.arrayIndex = arrayIndex;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	return count;
}

void HitboxData::InvalidateFrameCache()
{
	for (FrameCacheEntry& slot : g_frameCache)
		slot.valid = false;
}

bool HitboxData::ResolveAndReadBoxes(void* entity, FrameObject& outFrame, Box* outBoxes, int maxBoxes,
	int& outCount)
{
	if (entity == nullptr || outBoxes == nullptr || maxBoxes <= 0)
		return false;

	if (FrameCacheEntry* const cached = FindFrameCacheEntry(entity))
	{
		outFrame = cached->frame;
		outCount = (std::min)(cached->count, maxBoxes);
		memcpy(outBoxes, cached->boxes, sizeof(Box) * outCount);
		return true;
	}

	FrameObject frame = {};
	if (!Resolve(entity, frame))
		return false;

	Box boxes[kMaxBoxes] = {};
	const int count = ReadBoxes(frame, boxes, kMaxBoxes);

	FrameCacheEntry& slot = AllocateFrameCacheEntry(entity);
	slot.frame = frame;
	slot.count = count;
	memcpy(slot.boxes, boxes, sizeof(Box) * count);

	outFrame = frame;
	outCount = (std::min)(count, maxBoxes);
	memcpy(outBoxes, boxes, sizeof(Box) * outCount);
	return true;
}
