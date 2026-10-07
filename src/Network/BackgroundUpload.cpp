#include "Network/BackgroundUpload.h"

#include "Core/CodeFingerprint.h"
#include "Core/MeasuredCode.h"
#include "Core/Config/interfaces.h"
#include "Core/StateTimer.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/BattleProgress.h"
#include "Network/NetLink.h"
#include "Network/UploadChain.h"
#include "Network/UploadLedger.h"
#include "Overlay/Hud/NotificationBar.h"

#include <Windows.h>

#include <cstring>

namespace {

typedef bool(__fastcall* StartProfileUploadFn)(int slot);
typedef void*(__thiscall* DeleteRequestFn)(void* request, int flags);

constexpr int kDeleteAndFree = 1;
constexpr int kProfileSlot = 0;
constexpr int kResultOk = 0;
constexpr int kResultUnknown = -1;
constexpr int kChainUnread = -1;
constexpr uint8_t kNotLaunched = 0;
constexpr uint8_t kPublished = 1;

const CodeFingerprint kMeasuredCode[] = {
	{ GameOffsets::kSiteReplayMenuScoresStarted, { 0xC7, 0x46, 0x30, 0x07, 0x00, 0x00, 0x00 }, 7, 0, {}, 0 },
	{ GameOffsets::kSiteReplayMenuScoreStatus, { 0xB8 }, 1, GameOffsets::kScoreUploadStatus, {}, 0 },
	{ GameOffsets::kSiteReplayMenuMoreStatus, { 0xB8 }, 1, GameOffsets::kScoreUploadStatusMore, {}, 0 },
	{ GameOffsets::kSiteScoreUploadSlotRead, { 0x89, 0x95, 0x30, 0xFF, 0xFF, 0xFF, 0x8B, 0x04, 0xDD }, 9,
		GameOffsets::kWebRequestSlots + GameOffsets::kWebRequestSlotPointer, {}, 0 },
	{ GameOffsets::kSiteScoreUploadLaunched, { 0xC6, 0x04, 0xDD }, 3, GameOffsets::kWebRequestLaunches, { 0x01 }, 1 },
	{ GameOffsets::kSiteReplayMenuArmChain, { 0x83, 0x3D }, 2, GameOffsets::kReplayUploadChain, { 0x00 }, 1 },
	{ GameOffsets::kSiteReplayMenuChainDone, { 0x83, 0x3D }, 2, GameOffsets::kReplayUploadChain, { 0x06 }, 1 },
	{ GameOffsets::kSiteChainPublishFlag, { 0xC6, 0x05 }, 2, GameOffsets::kProfileReplayPublished, { 0x01 }, 1 },
	{ GameOffsets::kSiteChainPublishId, { 0xA3 }, 1, GameOffsets::kProfileReplayId, {}, 0 },
	{ GameOffsets::kSiteTitleCloudLaunched, { 0x80, 0x3C, 0xF5 }, 3, GameOffsets::kTitleCloudLaunches, { 0x00 }, 1 },
	{ GameOffsets::kFnStartReplaySlotUpload, { 0x55, 0x8B, 0xEC, 0xA1 }, 4,
		GameOffsets::kTitleCloudSlots + GameOffsets::kWebRequestSlotPointer, {}, 0 },
	{ GameOffsets::kFnStartProfileUpload, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0x04, 0xFD },
		13, GameOffsets::kTitleCloudSlots + GameOffsets::kWebRequestSlotPointer, {}, 0 },
};

enum class Readiness
{
	Unchecked,
	Ready,
	Refused
};

struct MenuReplay
{
	uintptr_t data;
	uint32_t size;
	uint32_t id;
};

struct Batch
{
	int uploads;
	int failed;
};

Readiness g_readiness = Readiness::Unchecked;
UploadLedger g_ledger;
UploadChain g_chain;
BattleProgress g_battle;
StateTimer g_menuTimer;
uintptr_t g_menu = 0;
uint32_t g_menuOpenedAt = 0;
bool g_chainTaken = false;
uint32_t g_replayId = 0;
Batch g_batch = {};

bool ReadDword(uintptr_t address, uint32_t& out)
{
	return address != 0 && TryReadDword(reinterpret_cast<const void*>(address), out);
}

bool WriteDword(uintptr_t address, uint32_t value)
{
	return address != 0 && TryWriteDword(reinterpret_cast<void*>(address), value);
}

bool WriteByte(uintptr_t address, uint8_t value)
{
	return address != 0 && TryWriteMemory(reinterpret_cast<void*>(address), &value, sizeof(value));
}

uintptr_t ReadPointer(uintptr_t address)
{
	uint32_t value = 0;
	return ReadDword(address, value) ? value : 0;
}

bool CodeIsMeasured()
{
	if (g_readiness != Readiness::Unchecked)
		return g_readiness == Readiness::Ready;

	const int mismatch = MeasuredCode::FirstMismatch(kMeasuredCode);
	g_readiness = mismatch == MeasuredCode::kAllMatch ? Readiness::Ready : Readiness::Refused;

	if (mismatch == MeasuredCode::kAllMatch)
		LOG("ReplayUpload: the replay menu matches the measured 1.40 code, uploads can finish in the background");
	else
		LOG("ReplayUpload: rva 0x%x is not the measured 1.40 code, uploads wait as the game does",
			static_cast<unsigned>(kMeasuredCode[mismatch].siteRva));

	return g_readiness == Readiness::Ready;
}

bool IsEnabled()
{
	return g_modVals.backgroundReplayUpload && CodeIsMeasured();
}

bool HasVTable(uintptr_t request, uintptr_t vtableRva)
{
	const uintptr_t vtable = RvaToAddress(vtableRva);
	return request != 0 && vtable != 0 && ReadPointer(request) == vtable;
}

bool IsKnownRequest(uintptr_t request)
{
	return HasVTable(request, GameOffsets::kRecordScoreVTable) ||
		HasVTable(request, GameOffsets::kTitleCloudUploadVTable);
}

bool DeleteRequest(uintptr_t request)
{
	__try
	{
		const DeleteRequestFn destroy = **reinterpret_cast<DeleteRequestFn**>(request);
		destroy(reinterpret_cast<void*>(request), kDeleteAndFree);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

class GameUploadRequests : public IUploadRequests
{
public:
	bool IsFinished(uintptr_t request) override
	{
		uint8_t finished = 0;
		return TryReadMemory(&finished, reinterpret_cast<const void*>(request + GameOffsets::kWebRequestFinished),
			sizeof(finished)) && finished != 0;
	}

	int Result(uintptr_t request) override
	{
		uint32_t result = static_cast<uint32_t>(kResultUnknown);
		ReadDword(request + GameOffsets::kWebRequestResult, result);
		return static_cast<int>(result);
	}

	void Release(uintptr_t request) override
	{
		if (!IsKnownRequest(request))
		{
			LOG("ReplayUpload: request 0x%p no longer looks like an upload, left in memory",
				reinterpret_cast<void*>(request));
			return;
		}

		if (!DeleteRequest(request))
			LOG("ReplayUpload: deleting request 0x%p faulted", reinterpret_cast<void*>(request));
	}
};

GameUploadRequests g_requests;

uintptr_t SlotAddress(uintptr_t tableRva, int slot)
{
	const uintptr_t table = RvaToAddress(tableRva);
	return table == 0 ? 0 : table + slot * GameOffsets::kWebRequestSlotStride;
}

uintptr_t RequestIn(uintptr_t slot)
{
	return slot == 0 ? 0 : ReadPointer(slot + GameOffsets::kWebRequestSlotPointer);
}

void ClearSlot(uintptr_t slot)
{
	WriteDword(slot, 0);
	WriteDword(slot + GameOffsets::kWebRequestSlotPointer, 0);
}

bool StartReplaySlotUpload(uintptr_t replayBytes, uint32_t replayLength)
{
	const uintptr_t function = CodeSignatures::Address(GameOffsets::kFnStartReplaySlotUpload);
	const int kind = GameOffsets::kReplaySlotUploadKind;
	uint8_t started = 0;

	if (function == 0)
		return false;

	__try
	{
		__asm
		{
			push replayLength
			push replayBytes
			mov edx, kind
			xor ecx, ecx
			call function
			add esp, 8
			mov started, al
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return started != 0;
}

bool StartProfileUpload()
{
	const auto start = reinterpret_cast<StartProfileUploadFn>(CodeSignatures::Address(GameOffsets::kFnStartProfileUpload));

	if (start == nullptr)
		return false;

	__try
	{
		return start(kProfileSlot);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

void AdoptScore(uintptr_t statusRva, int slot, uint32_t now)
{
	const uintptr_t status = RvaToAddress(statusRva) + slot * sizeof(uint32_t);
	uint32_t value = 0;

	if (!ReadDword(status, value) || value != GameOffsets::kScoreUploadPending)
		return;

	const uintptr_t requestSlot = SlotAddress(GameOffsets::kWebRequestSlots, slot);
	const uintptr_t request = RequestIn(requestSlot);

	if (!HasVTable(request, GameOffsets::kRecordScoreVTable) || !g_ledger.Adopt(request, UploadKind::Score, now))
		return;

	ClearSlot(requestSlot);

	const uintptr_t launch = SlotAddress(GameOffsets::kWebRequestLaunches, slot);
	WriteByte(launch, kNotLaunched);
	WriteDword(launch + GameOffsets::kWebRequestLaunchResult, kResultOk);
	WriteDword(status, GameOffsets::kScoreUploadDone);
	++g_batch.uploads;
}

void AdoptScores(uint32_t now)
{
	for (int slot = 0; slot < GameOffsets::kScoreUploadSlots; ++slot)
	{
		AdoptScore(GameOffsets::kScoreUploadStatus, slot, now);
		AdoptScore(GameOffsets::kScoreUploadStatusMore, slot, now);
	}
}

bool TitleCloudSlotBusy()
{
	const uintptr_t request = RequestIn(SlotAddress(GameOffsets::kTitleCloudSlots, kProfileSlot));
	return request != 0 && !g_requests.IsFinished(request);
}

bool LedgerHasRoom()
{
	return g_ledger.Pending() < UploadLedger::kCapacity;
}

bool AdoptTitleCloud(UploadKind kind, uint32_t now)
{
	const uintptr_t slot = SlotAddress(GameOffsets::kTitleCloudSlots, kProfileSlot);
	const uintptr_t request = RequestIn(slot);

	if (!HasVTable(request, GameOffsets::kTitleCloudUploadVTable) || !g_ledger.Adopt(request, kind, now))
		return false;

	ClearSlot(slot);
	WriteByte(RvaToAddress(GameOffsets::kTitleCloudLaunches), kNotLaunched);
	++g_batch.uploads;
	return true;
}

bool ReadMenuReplay(uintptr_t menu, MenuReplay& out)
{
	uint32_t capacity = 0;

	out.data = ReadPointer(menu + GameOffsets::kReplaySaveMenuData);

	return out.data != 0 && ReadDword(menu + GameOffsets::kReplaySaveMenuDataCapacity, capacity) &&
		ReadDword(menu + GameOffsets::kReplaySaveMenuDataSize, out.size) &&
		ReadDword(menu + GameOffsets::kReplaySaveMenuReplayId, out.id) && out.size != 0 && out.size <= capacity;
}

uint32_t ReadGameChain()
{
	uint32_t chain = static_cast<uint32_t>(kChainUnread);
	ReadDword(RvaToAddress(GameOffsets::kReplayUploadChain), chain);
	return chain;
}

void HandBackGameChain()
{
	const uint32_t chain = ReadGameChain();

	if (chain != static_cast<uint32_t>(GameOffsets::kReplayUploadChainDone))
	{
		LOG("ReplayUpload: the game's upload chain reads %d, left as it is", static_cast<int>(chain));
		return;
	}

	WriteDword(RvaToAddress(GameOffsets::kReplayUploadChain), GameOffsets::kReplayUploadChainIdle);
	LOG("ReplayUpload: the game's upload chain is idle again, the next match uploads its replay slot too");
}

void TakeChain(uintptr_t menu, uint32_t now)
{
	g_chainTaken = true;

	const uint32_t chain = ReadGameChain();

	if (chain != static_cast<uint32_t>(GameOffsets::kReplayUploadChainIdle))
	{
		LOG("ReplayUpload: the game's upload chain reads %d, not idle, so no replay slot upload this match",
			static_cast<int>(chain));
		return;
	}

	if (!g_chain.IsIdle() || !LedgerHasRoom() || TitleCloudSlotBusy())
	{
		LOG("ReplayUpload: another upload still runs, the replay slot upload is left to the game");
		return;
	}

	MenuReplay replay = {};

	if (!ReadMenuReplay(menu, replay))
		return;

	if (!StartReplaySlotUpload(replay.data, replay.size))
	{
		LOG("ReplayUpload: the replay slot upload did not start, the game will try it itself");
		return;
	}

	WriteDword(RvaToAddress(GameOffsets::kReplayUploadChain), GameOffsets::kReplayUploadChainDone);

	if (!AdoptTitleCloud(UploadKind::ReplaySlot, now))
	{
		LOG("ReplayUpload: the replay slot upload started but could not be taken over, the game keeps it");
		return;
	}

	g_replayId = replay.id;
	g_chain.ReplaySlotStarted();
	LOG("ReplayUpload: %u byte replay goes to the replay slot in the background", replay.size);
}

void ReleaseMenu(uintptr_t menu, uint32_t now)
{
	uint32_t state = 0;

	if (!ReadDword(menu + GameOffsets::kReplaySaveMenuState, state))
		return;

	const int current = static_cast<int>(state);

	if (current < GameOffsets::kReplaySaveMenuWaitScores || current > GameOffsets::kReplaySaveMenuArmChain)
		return;

	AdoptScores(now);

	if (!g_chainTaken)
		TakeChain(menu, now);
}

void PublishReplay()
{
	WriteByte(RvaToAddress(GameOffsets::kProfileReplayPublished), kPublished);
	WriteDword(RvaToAddress(GameOffsets::kProfileReplayId), g_replayId);
}

const char* KindName(UploadKind kind)
{
	switch (kind)
	{
	case UploadKind::Score:
		return "score and replay board";
	case UploadKind::ReplaySlot:
		return "replay slot";
	case UploadKind::Profile:
		return "profile";
	}

	return "unknown";
}

void Settle(const UploadLedger::Outcome& outcome)
{
	const int result = outcome.abandoned ? kResultUnknown : outcome.result;

	if (outcome.abandoned || result != kResultOk)
		++g_batch.failed;

	LOG("ReplayUpload: %s upload %s after %u ms, result %d", KindName(outcome.kind),
		outcome.abandoned ? "abandoned" : "finished", outcome.elapsedMs, result);

	if (outcome.kind == UploadKind::ReplaySlot && g_chain.ReplaySlotFinished(result))
		PublishReplay();

	if (outcome.kind == UploadKind::Profile)
		g_chain.ProfileFinished();
}

void CollectOutcomes(uint32_t now)
{
	UploadLedger::Outcome outcomes[UploadLedger::kCapacity] = {};
	const int count = g_ledger.Collect(g_requests, now, outcomes, UploadLedger::kCapacity);

	for (int i = 0; i < count; ++i)
		Settle(outcomes[i]);
}

void StartProfileWhenDue(uint32_t now)
{
	if (!g_chain.ProfileDue(g_battle.IsAdvancing(now)) || !LedgerHasRoom() || TitleCloudSlotBusy())
		return;

	if (!StartProfileUpload() || !AdoptTitleCloud(UploadKind::Profile, now))
	{
		g_chain.ProfileNotStarted();
		LOG("ReplayUpload: the profile upload did not start");
		return;
	}

	g_chain.ProfileStarted();
}

void ReportWhenDrained()
{
	if (g_batch.uploads == 0 || g_ledger.Pending() != 0 || !g_chain.IsIdle())
		return;

	LOG("ReplayUpload: background uploads done, %d of %d failed", g_batch.failed, g_batch.uploads);

	if (g_batch.failed == 0)
		NotificationBar::Add("Replay uploaded in the background");
	else
		NotificationBar::Add("Replay upload: %d of %d step(s) failed, see the log", g_batch.failed, g_batch.uploads);

	g_batch = {};
}

void WatchMenu(uintptr_t menu, uint32_t now)
{
	if (menu != g_menu)
	{
		if (g_menu != 0)
			LOG("ReplayUpload: replay menu closed after %u ms", now - g_menuOpenedAt);

		g_menu = menu;
		g_menuOpenedAt = now;
		g_menuTimer.Reset();
		g_chainTaken = false;
	}

	uint32_t state = 0;

	if (menu == 0 || !ReadDword(menu + GameOffsets::kReplaySaveMenuState, state))
		return;

	StateTimer::Change change = {};

	if (g_menuTimer.Observe(static_cast<int>(state), now, change))
		LOG("ReplayUpload: replay menu state %d -> %d after %u ms", change.from, change.to, change.heldMs);
}

uintptr_t CurrentMenu()
{
	return ReadPointer(RvaToAddress(GameOffsets::kReplaySaveMenu));
}

}

void BackgroundUpload::OnFrame()
{
	if (!IsMeasuredGameBuild())
		return;

	const uint32_t now = GetTickCount();
	const NetLink::Snapshot& link = NetLink::Current();
	const uintptr_t menu = CurrentMenu();

	const bool enabled = IsEnabled();

	g_battle.Observe(link.netplayActive, link.netplayFrame, now);
	WatchMenu(menu, now);

	if (menu != 0 && enabled)
		ReleaseMenu(menu, now);

	if (g_ledger.Pending() == 0 && g_chain.IsIdle())
		return;

	CollectOutcomes(now);
	StartProfileWhenDue(now);
	ReportWhenDrained();

	if (g_chain.TakeGameChainHandBack())
		HandBackGameChain();
}

int BackgroundUpload::Pending()
{
	return g_ledger.Pending();
}
