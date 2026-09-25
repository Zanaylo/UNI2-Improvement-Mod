#include "Network/Spectate/SpectateMatch.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/BgCeiling.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Patches/GamePatches.h"
#include "Game/Stages/StageLibrary.h"
#include "Network/ModHandshake.h"
#include "Game/Engine/CodeSignatures.h"

#include <Windows.h>

#include <cstring>
#include <vector>

namespace {

typedef void(__cdecl* MatchSetupFn)();
typedef void(__fastcall* ApplyBlockFn)(int32_t*);

constexpr int kSides = 2;
constexpr int kSlotsPerSide = 3;
constexpr int kStageSlot = 6;
constexpr int kBgmSlot = 7;
constexpr int kRuleSlot = 8;
constexpr int kFallbackStage = 1;

constexpr uintptr_t kRules[] = {
	GameOffsets::kBattleRuleA, GameOffsets::kBattleRuleB, GameOffsets::kBattleRuleC, GameOffsets::kBattleRuleD,
};

int32_t ReadInt(uintptr_t rva)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value);

	return static_cast<int32_t>(value);
}

uint8_t ReadByte(uintptr_t rva)
{
	uint8_t value = 0;
	TryReadMemory(&value, reinterpret_cast<const void*>(RvaToAddress(rva)), sizeof(value));

	return value;
}

bool WriteInt(uintptr_t rva, int32_t value)
{
	return TryWriteDword(reinterpret_cast<void*>(RvaToAddress(rva)), static_cast<uint32_t>(value));
}

bool ReadBlock(void* out, uintptr_t rva, size_t size)
{
	return TryReadMemory(out, reinterpret_cast<const void*>(RvaToAddress(rva)), size);
}

bool WriteBlock(const void* in, uintptr_t rva, size_t size)
{
	return TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(rva)), in, size);
}

void CaptureBlock(int32_t* block)
{
	for (int side = 0; side < kSides; ++side)
	{
		const int base = side * kSlotsPerSide;

		block[base] = ReadInt(GameOffsets::kBattleCharaRecord + side * GameOffsets::kBattleCharaRecordStride);
		block[base + 1] = ReadInt(GameOffsets::kBattleCharaColor + side * GameOffsets::kBattleCharaSlotStride);
		block[base + 2] = ReadInt(GameOffsets::kBattleCharaExtra + side * GameOffsets::kBattleCharaSlotStride);
	}

	block[kStageSlot] = ReadInt(GameOffsets::kBgPendingNumber);
	block[kBgmSlot] = ReadInt(GameOffsets::kBattleBgm);

	for (int i = 0; i < static_cast<int>(sizeof(kRules) / sizeof(kRules[0])); ++i)
		block[kRuleSlot + i] = ReadInt(kRules[i]);
}

int PlayableStage(int stage)
{
	const uintptr_t* const table = reinterpret_cast<const uintptr_t*>(BgCeiling::RecordTable());

	if (table == nullptr || stage <= 0 || stage >= BgCeiling::Numbers())
		return kFallbackStage;

	uintptr_t record = 0;

	if (!TryReadMemory(&record, table + stage, sizeof(record)) || record == 0)
		return kFallbackStage;

	return stage;
}

int SlotNamed(const char* name)
{
	if (name[0] == 0)
		return kFallbackStage;

	std::vector<StageLibrary::Entry> entries;
	StageLibrary::Snapshot(entries);

	for (const StageLibrary::Entry& entry : entries)
	{
		if (entry.shown && entry.name == name)
			return entry.slot;
	}

	return kFallbackStage;
}

int LocalStage(const SpectateMatch::Snapshot& in)
{
	const int stage = in.block[kStageSlot];

	return PlayableStage(StageLibrary::GameOwns(stage) ? stage : SlotNamed(in.stageName));
}

int HostStage()
{
	const int own = ReadInt(GameOffsets::kStageOwnPick);
	const bool ownShown = ReadByte(GameOffsets::kStageSyncOption) != 0 &&
		ReadByte(GameOffsets::kOnlineMatchFlag) != 0 && own > 0;

	return ownShown ? own : ReadInt(GameOffsets::kBgPendingNumber);
}

bool RunSetup(int32_t* block)
{
	__try
	{
		reinterpret_cast<MatchSetupFn>(CodeSignatures::Address(GameOffsets::kFnRankMatchSetup))();
		reinterpret_cast<ApplyBlockFn>(CodeSignatures::Address(GameOffsets::kFnApplyMatchBlock))(block);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

}

bool SpectateMatch::Capture(Snapshot& out)
{
	memset(&out, 0, sizeof(out));

	if (!ReadBlock(out.records, GameOffsets::kMatchRecords, sizeof(out.records)) ||
		!ReadBlock(out.room, GameOffsets::kRoomSettings, sizeof(out.room)))
	{
		return false;
	}

	CaptureBlock(out.block);

	out.localSide = ReadInt(GameOffsets::kMatchLocalSide);
	out.remoteSide = ReadInt(GameOffsets::kMatchRemoteSide);
	out.seed = ReadInt(GameOffsets::kMatchSeedCounter);
	out.settingsWin = ReadByte(GameOffsets::kMatchSettingsWin);

	ModHandshake::DescribeData(GamePatches::BootIndex(), out.patch, sizeof(out.patch));
	ResolveStage(out);

	return true;
}

void SpectateMatch::ResolveStage(Snapshot& snapshot)
{
	const int stage = HostStage();

	snapshot.block[kStageSlot] = stage;
	memset(snapshot.stageName, 0, sizeof(snapshot.stageName));

	StageLibrary::Entry entry = {};

	if (StageLibrary::GameOwns(stage) || !StageLibrary::Of(StageLibrary::IdForSlot(stage), entry))
		return;

	strncpy_s(snapshot.stageName, entry.name.c_str(), _TRUNCATE);
}

bool SpectateMatch::Apply(const Snapshot& in)
{
	Snapshot copy = in;
	copy.stageName[kStageNameBytes - 1] = 0;
	copy.patch[kPatchBytes - 1] = 0;

	const int stage = LocalStage(copy);

	if (stage != in.block[kStageSlot])
		LOG("SpectateMatch: the host's stage %d '%s' plays as stage %d here", in.block[kStageSlot], copy.stageName, stage);

	copy.block[kStageSlot] = stage;

	if (!WriteBlock(copy.room, GameOffsets::kRoomSettings, sizeof(copy.room)) ||
		!WriteBlock(copy.records, GameOffsets::kMatchRecords, sizeof(copy.records)) ||
		!WriteBlock(&copy.settingsWin, GameOffsets::kMatchSettingsWin, sizeof(copy.settingsWin)) ||
		!WriteInt(GameOffsets::kMatchLocalSide, copy.localSide) ||
		!WriteInt(GameOffsets::kMatchRemoteSide, copy.remoteSide) ||
		!WriteInt(GameOffsets::kMatchKind, GameOffsets::kMatchKindRank) ||
		!WriteInt(GameOffsets::kMatchSeedCounter, copy.seed))
	{
		LOG("SpectateMatch: the match records could not be written");
		return false;
	}

	if (!RunSetup(copy.block))
	{
		LOG("SpectateMatch: the game's match setup faulted");
		return false;
	}

	WriteInt(GameOffsets::kStageOwnPick, stage);
	WriteInt(GameOffsets::kMatchLocalSide, GameOffsets::kMatchSpectatorSide);

	LOG("SpectateMatch: match applied, characters %d and %d, stage %d, data %s", copy.block[0], copy.block[kSlotsPerSide],
		stage, copy.patch);
	return true;
}

int SpectateMatch::StageOf(const Snapshot& snapshot)
{
	return snapshot.block[kStageSlot];
}

const char* SpectateMatch::StageNameOf(const Snapshot& snapshot)
{
	return snapshot.stageName;
}

const char* SpectateMatch::PatchOf(const Snapshot& snapshot)
{
	return snapshot.patch;
}
