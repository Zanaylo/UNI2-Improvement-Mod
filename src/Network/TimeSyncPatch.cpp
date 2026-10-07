#include "Network/TimeSyncPatch.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Network/TimeSyncTuning.h"

#include <cstring>

namespace {

const float g_halvedAverageDivisor = TimeSyncTuning::kHalvedAverageDivisor;

int PlanAll(TimeSyncTuning::Patch* patches, int capacity)
{
	const int count = TimeSyncTuning::Plan(g_modVals.timeSyncInterval, g_modVals.timeSyncTail, patches, capacity);

	if (!g_modVals.timeSyncHalveGap)
		return count;

	return count + TimeSyncTuning::PlanGapHalving(GetGameBaseAddress(),
		reinterpret_cast<uintptr_t>(&g_halvedAverageDivisor), patches + count, capacity - count);
}

const char* GapText()
{
	return g_modVals.timeSyncHalveGap ? "half the gap, as stock GGPO" : "the whole gap, as the game";
}

uint8_t* OperandOf(const TimeSyncTuning::Patch& patch)
{
	const uintptr_t site = CodeSignatures::Address(patch.siteRva);

	if (!IsAddressInGameModule(site))
		return nullptr;

	return reinterpret_cast<uint8_t*>(site + patch.operandOffset);
}

bool StillAsMeasured(const TimeSyncTuning::Patch& patch)
{
	uint8_t* const operand = OperandOf(patch);
	uint8_t current[TimeSyncTuning::kMostPatchBytes] = {};

	return operand != nullptr && TryReadMemory(current, operand, patch.length) &&
		memcmp(current, patch.expected, patch.length) == 0;
}

bool AllStillAsMeasured(const TimeSyncTuning::Patch* patches, int count)
{
	for (int i = 0; i < count; ++i)
	{
		if (!StillAsMeasured(patches[i]))
			return false;
	}

	return true;
}

int Apply(const TimeSyncTuning::Patch* patches, int count)
{
	int written = 0;

	for (int i = 0; i < count; ++i)
	{
		if (WriteCodeBytes(OperandOf(patches[i]), patches[i].wanted, patches[i].length))
			++written;
	}

	return written;
}

}

bool TimeSyncPatch::Install()
{
	TimeSyncTuning::Patch patches[TimeSyncTuning::kMostPatches] = {};
	const int count = PlanAll(patches, TimeSyncTuning::kMostPatches);

	if (count == 0)
	{
		LOG("TimeSync: the game's own pacing, a check every %d frames with its tail %s, correcting %s",
			g_modVals.timeSyncInterval, g_modVals.timeSyncTail ? "on" : "off", GapText());
		return false;
	}

	if (!AllStillAsMeasured(patches, count))
	{
		LOG("TimeSync: the pacing code is not the measured 1.40 bytes, left alone");
		return false;
	}

	const int written = Apply(patches, count);

	LOG("TimeSync: %d of %d change(s) written, a check every %d frames, tail %s, correcting %s", written, count,
		g_modVals.timeSyncInterval, g_modVals.timeSyncTail ? "kept" : "dropped", GapText());
	return written == count;
}
