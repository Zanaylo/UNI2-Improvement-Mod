#include "Network/TimeSyncTuning.h"

#include "Game/Engine/GameOffsets.h"

#include <cstring>

namespace {

constexpr size_t kImmediateOffset = 1;
constexpr size_t kLoadOperandOffset = 4;
constexpr size_t kImmediateBytes = 4;
constexpr size_t kBranchBytes = 1;
constexpr uint8_t kJumpIfBelowOrEqualShort = 0x76;
constexpr uint8_t kJumpShort = 0xEB;

constexpr uintptr_t kIntervalSites[] = {
	GameOffsets::kSiteTimeSyncSpread,
	GameOffsets::kSiteTimeSyncNextCheck,
	GameOffsets::kSiteTimeSyncRearmSpread,
};

constexpr int kIntervalSiteCount = static_cast<int>(sizeof(kIntervalSites) / sizeof(kIntervalSites[0]));

TimeSyncTuning::Patch IntervalPatch(uintptr_t siteRva, int interval)
{
	TimeSyncTuning::Patch patch = {};
	const uint32_t game = static_cast<uint32_t>(TimeSyncTuning::kGameInterval);
	const uint32_t wanted = static_cast<uint32_t>(interval);

	patch.siteRva = siteRva;
	patch.operandOffset = kImmediateOffset;
	patch.length = kImmediateBytes;
	memcpy(patch.expected, &game, kImmediateBytes);
	memcpy(patch.wanted, &wanted, kImmediateBytes);
	return patch;
}

TimeSyncTuning::Patch TailPatch()
{
	TimeSyncTuning::Patch patch = {};

	patch.siteRva = GameOffsets::kSiteTimeSyncTail;
	patch.length = kBranchBytes;
	patch.expected[0] = kJumpIfBelowOrEqualShort;
	patch.wanted[0] = kJumpShort;
	return patch;
}

bool IsSafeInterval(int interval)
{
	return interval >= TimeSyncTuning::kShortestInterval && interval <= TimeSyncTuning::kGameInterval;
}

}

int TimeSyncTuning::Plan(int interval, bool keepTail, Patch* out, int capacity)
{
	if (out == nullptr || !IsSafeInterval(interval))
		return 0;

	const bool changeInterval = interval != kGameInterval;
	const int needed = (changeInterval ? kIntervalSiteCount : 0) + (keepTail ? 0 : 1);

	if (needed > capacity)
		return 0;

	int count = 0;

	if (changeInterval)
	{
		for (uintptr_t site : kIntervalSites)
			out[count++] = IntervalPatch(site, interval);
	}

	if (!keepTail)
		out[count++] = TailPatch();

	return count;
}

int TimeSyncTuning::PlanGapHalving(uintptr_t gameBase, uintptr_t halvedDivisor, Patch* out, int capacity)
{
	if (out == nullptr || capacity < 1 || gameBase == 0 || halvedDivisor == 0)
		return 0;

	const uint32_t game = static_cast<uint32_t>(gameBase + GameOffsets::kTimeSyncAverageDivisor);
	const uint32_t wanted = static_cast<uint32_t>(halvedDivisor);

	out[0] = {};
	out[0].siteRva = GameOffsets::kSiteTimeSyncAverages;
	out[0].operandOffset = kLoadOperandOffset;
	out[0].length = kImmediateBytes;
	memcpy(out[0].expected, &game, kImmediateBytes);
	memcpy(out[0].wanted, &wanted, kImmediateBytes);
	return 1;
}
