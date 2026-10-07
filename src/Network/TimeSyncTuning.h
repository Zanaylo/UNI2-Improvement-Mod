#pragma once

#include <cstddef>
#include <cstdint>

namespace TimeSyncTuning
{
	constexpr int kGameInterval = 240;
	constexpr int kShortestInterval = 60;
	constexpr int kMostPatches = 5;
	constexpr float kHalvedAverageDivisor = 80.0f;
	constexpr size_t kMostPatchBytes = 4;

	struct Patch
	{
		uintptr_t siteRva;
		size_t operandOffset;
		size_t length;
		uint8_t expected[kMostPatchBytes];
		uint8_t wanted[kMostPatchBytes];
	};

	int Plan(int interval, bool keepTail, Patch* out, int capacity);
	int PlanGapHalving(uintptr_t gameBase, uintptr_t halvedDivisor, Patch* out, int capacity);
}
