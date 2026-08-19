// Runtime hitbox layout, recovered from the HA6 chunk parser and the engine's own teardown routine.

#pragma once

#include <cstdint>

namespace HitboxData
{
	constexpr int kArrayCount = 4;
	constexpr int kMaxBoxesPerArray = 33;
	constexpr int kMaxBoxes = kArrayCount * kMaxBoxesPerArray;

	enum BoxArray
	{
		BoxArray_If = 0,
		BoxArray_Ef = 1,
		BoxArray_Normal = 2,
		BoxArray_Attack = 3
	};

	constexpr int kFirstBoxArray = BoxArray_Normal;

	struct Box
	{
		int x1;
		int y1;
		int x2;
		int y2;
		int index;
		int arrayIndex;
	};

	struct FrameObject
	{
		void* pointer;
		uintptr_t offsetInPlayerData;
		void* arrays[kArrayCount];
		int counts[kArrayCount];
		uint32_t existFlags;
	};

	struct ScanResult
	{
		bool found;
		uintptr_t offset;
		int candidates;
		int bestScore;
	};

	bool Resolve(void* playerData, FrameObject& out);
	int ReadBoxes(const FrameObject& frameObject, Box* outBoxes, int maxBoxes);

	ScanResult ScanForFrameObject(void* playerData);
}
