#pragma once

#include "Game/GameOffsets.h"

#include <cstdint>

namespace SpectateMatch
{
	constexpr int kBlockSlots = 12;
	constexpr int kStageNameBytes = 64;
	constexpr int kPatchBytes = 24;

#pragma pack(push, 1)
	struct Snapshot
	{
		uint8_t records[GameOffsets::kMatchRecordsSize];
		uint8_t room[GameOffsets::kRoomSettingsSize];
		int32_t block[kBlockSlots];
		int32_t localSide;
		int32_t remoteSide;
		int32_t seed;
		uint8_t settingsWin;
		char stageName[kStageNameBytes];
		char patch[kPatchBytes];
	};
#pragma pack(pop)

	bool Capture(Snapshot& out);
	void ResolveStage(Snapshot& snapshot);
	bool Apply(const Snapshot& in);

	int StageOf(const Snapshot& snapshot);
	const char* StageNameOf(const Snapshot& snapshot);
	const char* PatchOf(const Snapshot& snapshot);
}
