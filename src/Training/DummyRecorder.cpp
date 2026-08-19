#include "Training/DummyRecorder.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Hooks/HookManager.h"
#include "Training/FrameMeter.h"
#include "Training/FrameStepper.h"

#include <MinHook.h>
#include <new>
#include <Windows.h>

namespace {

using Promote_t = void(__fastcall*)(void*, void*);

Promote_t oPromote = nullptr;
bool g_installed = false;

volatile uint64_t g_callCount = 0;
volatile uint64_t g_deferred = 0;

bool g_hasPending = false;
uintptr_t g_frameCounterRva = 0;

constexpr int kReversalHoldFrames = 40;

int g_reversalHold = 0;
bool g_reversalSawFree = false;
bool g_reversalWaiting = false;

constexpr int kRestartDebounceFrames = 20;

bool g_hasFrameCounter = false;
uint32_t g_lastFrameCounter = 0;
int g_restartDebounce = 0;
volatile uint64_t g_restartCount = 0;

bool DetectRestart()
{
	uint32_t counter = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kFrameCounterA)),
		counter))
	{
		return false;
	}

	if (g_restartDebounce > 0)
		--g_restartDebounce;

	const uint32_t previous = g_lastFrameCounter;
	const bool had = g_hasFrameCounter;

	g_hasFrameCounter = true;
	g_lastFrameCounter = counter;

	if (!had || counter >= previous || g_restartDebounce > 0)
		return false;

	g_restartDebounce = kRestartDebounceFrames;
	++g_restartCount;
	return true;
}
uint32_t g_reversalModeBefore = 0;
uint32_t g_actionMode = 0;

bool g_restartAtEnd = false;
bool g_replaying = false;
void* g_pendingThis = nullptr;
void* g_pendingUnused = nullptr;
int g_leadInRemaining = 0;
int g_leadInLength = 0;

struct CallSite
{
	uintptr_t returnRva;
	uint64_t calls;
};

CallSite g_callSites[DummyRecorder::kMaxCallSites] = {};
int g_callSiteCount = 0;

struct Transition
{
	uint32_t from;
	uint32_t to;
};

Transition g_history[DummyRecorder::kHistory] = {};
int g_historyCount = 0;

uint32_t g_state = 0;
uint32_t g_fieldB = 0;
uint32_t g_fieldC = 0;
uint32_t g_actionSetting = 0;

bool g_hasState = false;
uint32_t g_lastState = 0;

uint32_t ReadDword(uintptr_t rva)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value);
	return value;
}

void CountCallSite(uintptr_t returnAddress)
{
	const uintptr_t base = GetGameBaseAddress();
	if (base == 0 || returnAddress < base)
		return;

	const uintptr_t rva = returnAddress - base;

	for (int i = 0; i < g_callSiteCount; ++i)
	{
		if (g_callSites[i].returnRva == rva)
		{
			++g_callSites[i].calls;
			return;
		}
	}

	if (g_callSiteCount >= DummyRecorder::kMaxCallSites)
		return;

	g_callSites[g_callSiteCount].returnRva = rva;
	g_callSites[g_callSiteCount].calls = 1;
	++g_callSiteCount;

	LOG("DummyRecorder: new call site, return rva 0x%06x", (unsigned)rva);
}

void RecordTransition(uint32_t from, uint32_t to)
{
	for (int i = DummyRecorder::kHistory - 1; i > 0; --i)
		g_history[i] = g_history[i - 1];

	g_history[0].from = from;
	g_history[0].to = to;

	if (g_historyCount < DummyRecorder::kHistory)
		++g_historyCount;

	LOG("DummyRecorder: state %u -> %u (action setting %u)", from, to, g_actionSetting);
}

bool WantsLeadIn()
{
	return FrameMeter::GetAutoPause().onDummyRecord;
}

void __fastcall HookedPromote(void* thisPtr, void* unused)
{
	++g_callCount;
	CountCallSite(reinterpret_cast<uintptr_t>(_ReturnAddress()));

	if (g_replaying || !GameState::AllowsTrainingTools() || !WantsLeadIn())
	{
		oPromote(thisPtr, unused);
		return;
	}

	const FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();
	const int frames = config.resumeDelayFrames;

	oPromote(thisPtr, unused);

	if (frames <= 0)
		return;

	g_pendingThis = thisPtr;
	g_pendingUnused = unused;
	g_restartAtEnd = true;

	if (config.leadInMode == FrameMeter::LeadIn_Still)
		FrameStepper::FreezeFor(frames, FrameStepper::FreezeMode::TickSuppress);

	g_hasPending = true;
	g_leadInLength = frames;
	g_leadInRemaining = frames;
	++g_deferred;
}

}

bool DummyRecorder::Install()
{
	if (g_installed)
		return true;

	void* target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnDummyPromote));
	if (target == nullptr)
		return false;

	if (!HookManager::CreateHook(target, &HookedPromote, reinterpret_cast<void**>(&oPromote),
		"DummyRecorder::Promote"))
	{
		return false;
	}

	if (!HookManager::EnableAllHooks())
		return false;

	g_installed = true;
	LOG("DummyRecorder: hooked 0x%06x", (unsigned)GameOffsets::kFnDummyPromote);
	return true;
}

bool DummyRecorder::IsInstalled()
{
	return g_installed;
}

namespace {

struct ForcedSlot
{
	uintptr_t rva;
	uint32_t value;
	uint32_t current;
	bool applied;
	uint32_t before;
};

ForcedSlot g_forced[DummyRecorder::kForcedSlots] = {};

void UpdateForcedSlots()
{
	const bool allowed = GameState::AllowsTrainingTools();

	for (int i = 0; i < DummyRecorder::kForcedSlots; ++i)
	{
		ForcedSlot& slot = g_forced[i];

		if (slot.rva == 0)
			continue;

		const uintptr_t address = RvaToAddress(slot.rva);
		slot.current = ReadDword(slot.rva);

		if (!allowed)
			continue;

		if (!slot.applied)
		{
			slot.before = slot.current;
			slot.applied = true;
		}

		if (slot.current != slot.value)
			TryWriteDword(reinterpret_cast<void*>(address), slot.value);
	}
}

void UpdateReversalAfterRestart()
{
	const uintptr_t address = RvaToAddress(GameOffsets::kDummyActionMode);
	g_actionMode = ReadDword(GameOffsets::kDummyActionMode);

	if (g_reversalHold > 0)
	{
		--g_reversalHold;

		uint32_t side = 0;
		TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kPlayerSideIndex)),
			side);

		const int dummy = side == 0 ? 1 : 0;
		const bool free = FrameMeter::IsPlayerFree(dummy);

		if (free)
			g_reversalSawFree = true;

		const bool committed = g_reversalSawFree && !free;

		if (g_reversalHold > 0 && !committed)
		{
			if (g_actionMode != GameOffsets::kDummyActionModeReversal)
				TryWriteDword(reinterpret_cast<void*>(address),
					GameOffsets::kDummyActionModeReversal);

			return;
		}

		g_reversalHold = 0;
		TryWriteDword(reinterpret_cast<void*>(address), g_reversalModeBefore);
		return;
	}

	const FrameMeter::AutoPauseConfig config = FrameMeter::GetAutoPause();

	if (g_reversalWaiting)
	{
		if (FrameStepper::GetResumeCountdown() > 0)
			return;

		g_reversalWaiting = false;
		g_reversalModeBefore = g_actionMode;
		g_reversalHold = kReversalHoldFrames;
		g_reversalSawFree = false;
		return;
	}

	const bool restarted = DetectRestart();

	if (!restarted || !config.reversalAfterRestart || !GameState::AllowsTrainingTools())
		return;

	if (config.reversalAfterCountdown && config.resumeDelayFrames > 0)
	{
		FrameStepper::FreezeFor(config.resumeDelayFrames, FrameStepper::FreezeMode::StopTime);

		if (FrameStepper::GetResumeCountdown() > 0)
		{
			g_reversalWaiting = true;
			LOG("DummyRecorder: restart - counting %d frames before arming the reversal",
				config.resumeDelayFrames);
			return;
		}
	}

	g_reversalModeBefore = g_actionMode;
	g_reversalHold = kReversalHoldFrames;
	g_reversalSawFree = false;
	LOG("DummyRecorder: restart at frame counter %u - holding the dummy action mode at reversal "
		"for up to %d frames", g_lastFrameCounter, kReversalHoldFrames);
}

}

void DummyRecorder::Update()
{
	UpdateForcedSlots();
	UpdateReversalAfterRestart();

	g_state = ReadDword(GameOffsets::kDummyState);
	g_fieldB = ReadDword(GameOffsets::kDummyStateB);
	g_fieldC = ReadDword(GameOffsets::kDummyStateC);
	g_actionSetting = ReadDword(GameOffsets::kDummyActionSetting);

	if (g_hasPending)
	{
		if (g_leadInRemaining > 0)
			--g_leadInRemaining;

		if (g_leadInRemaining <= 0)
		{
			g_hasPending = false;

			if (g_restartAtEnd)
			{
				g_restartAtEnd = false;
				g_replaying = true;
				oPromote(g_pendingThis, g_pendingUnused);
				g_replaying = false;
			}

			if (g_frameCounterRva != 0)
			{
				const uintptr_t address = RvaToAddress(g_frameCounterRva);
				uint32_t current = 0;

				if (TryReadDword(reinterpret_cast<const void*>(address), current))
				{
					LOG("DummyRecorder: frame counter %u -> 0 at the end of the lead-in", current);
					TryWriteDword(reinterpret_cast<void*>(address), 0);
				}
			}
		}
	}

	if (!g_hasState)
	{
		g_hasState = true;
		g_lastState = g_state;
		return;
	}

	if (g_state == g_lastState)
		return;

	const uint32_t previous = g_lastState;
	g_lastState = g_state;
	RecordTransition(previous, g_state);
}

uint32_t DummyRecorder::GetState()
{
	return g_state;
}

uint32_t DummyRecorder::GetFieldB()
{
	return g_fieldB;
}

uint32_t DummyRecorder::GetFieldC()
{
	return g_fieldC;
}

uint32_t DummyRecorder::GetActionSetting()
{
	return g_actionSetting;
}

uint32_t DummyRecorder::GetReversalMove(int index)
{
	if (index < 0 || index >= GameOffsets::kReversalSlotCount)
		return 0;

	return ReadDword(GameOffsets::kReversalMoves + index * 4);
}

void DummyRecorder::SetForced(int slot, uintptr_t rva, uint32_t value)
{
	if (slot < 0 || slot >= kForcedSlots)
		return;

	ForcedSlot& entry = g_forced[slot];

	if (entry.applied && entry.rva != 0 && (entry.rva != rva || rva == 0))
		TryWriteDword(reinterpret_cast<void*>(RvaToAddress(entry.rva)), entry.before);

	if (entry.rva != rva)
		entry.applied = false;

	entry.rva = rva;
	entry.value = value;
}

bool DummyRecorder::GetForced(int slot, uintptr_t& outRva, uint32_t& outValue, uint32_t& outCurrent)
{
	if (slot < 0 || slot >= kForcedSlots)
		return false;

	outRva = g_forced[slot].rva;
	outValue = g_forced[slot].value;
	outCurrent = g_forced[slot].current;
	return true;
}

uint32_t DummyRecorder::GetActionMode()
{
	return g_actionMode;
}

int DummyRecorder::GetReversalHoldRemaining()
{
	return g_reversalHold;
}

uint64_t DummyRecorder::GetRestartCount()
{
	return g_restartCount;
}

uint32_t DummyRecorder::GetReversalEnabled(int index)
{
	if (index < 0 || index >= GameOffsets::kReversalSlotCount)
		return 0;

	return ReadDword(GameOffsets::kReversalEnabled + index * 4);
}

uint64_t DummyRecorder::GetCallCount()
{
	return g_callCount;
}

uint64_t DummyRecorder::GetDeferredCount()
{
	return g_deferred;
}

int DummyRecorder::GetLeadInRemaining()
{
	return g_hasPending ? g_leadInRemaining : 0;
}

int DummyRecorder::GetLeadInLength()
{
	return g_leadInLength;
}

int DummyRecorder::GetCallSiteCount()
{
	return g_callSiteCount;
}

bool DummyRecorder::GetCallSite(int index, uintptr_t& outReturnRva, uint64_t& outCalls)
{
	if (index < 0 || index >= g_callSiteCount)
		return false;

	outReturnRva = g_callSites[index].returnRva;
	outCalls = g_callSites[index].calls;
	return true;
}

namespace {

constexpr size_t kRuntimeDwords =
	(DummyRecorder::kWatchRuntimeEnd - DummyRecorder::kWatchRuntimeStart) / 4;
constexpr size_t kSettingsDwords =
	(DummyRecorder::kWatchSettingsEnd - DummyRecorder::kWatchSettingsStart) / 4;
constexpr size_t kCharaDwords =
	(DummyRecorder::kWatchCharaEnd - DummyRecorder::kWatchCharaStart) / 4;
constexpr size_t kWatchDwords = kRuntimeDwords + kSettingsDwords + kCharaDwords;

uintptr_t WatchAddress(size_t index)
{
	if (index < kRuntimeDwords)
		return DummyRecorder::kWatchRuntimeStart + index * 4;

	if (index < kRuntimeDwords + kSettingsDwords)
		return DummyRecorder::kWatchSettingsStart + (index - kRuntimeDwords) * 4;

	return DummyRecorder::kWatchCharaStart + (index - kRuntimeDwords - kSettingsDwords) * 4;
}

uint32_t* g_snapshot = nullptr;
bool g_hasSnapshot = false;

struct Change
{
	uintptr_t rva;
	uint32_t before;
	uint32_t after;
	int length;
};

Change g_changes[DummyRecorder::kMaxChanges] = {};
int g_changeCount = 0;

}

void DummyRecorder::SetFrameCounterRva(uintptr_t rva)
{
	g_frameCounterRva = rva;
}

uintptr_t DummyRecorder::GetFrameCounterRva()
{
	return g_frameCounterRva;
}

void DummyRecorder::SnapshotRegion()
{
	if (g_snapshot == nullptr)
		g_snapshot = new (std::nothrow) uint32_t[kWatchDwords];

	if (g_snapshot == nullptr)
		return;

	for (size_t i = 0; i < kWatchDwords; ++i)
		g_snapshot[i] = ReadDword(WatchAddress(i));

	g_hasSnapshot = true;
	g_changeCount = 0;
}

void DummyRecorder::RefreshChanges()
{
	g_changeCount = 0;

	if (!g_hasSnapshot || g_snapshot == nullptr)
		return;

	for (size_t i = 0; i < kWatchDwords && g_changeCount < kMaxChanges; ++i)
	{
		const uint32_t now = ReadDword(WatchAddress(i));
		if (now == g_snapshot[i])
			continue;

		Change& change = g_changes[g_changeCount];
		change.rva = WatchAddress(i);
		change.before = g_snapshot[i];
		change.after = now;
		change.length = 1;

		while (i + 1 < kWatchDwords)
		{

			if (WatchAddress(i + 1) != WatchAddress(i) + 4)
				break;

			const uint32_t next = ReadDword(WatchAddress(i + 1));
			if (next == g_snapshot[i + 1] || next != change.after)
				break;

			++change.length;
			++i;
		}

		++g_changeCount;
	}
}

bool DummyRecorder::HasSnapshot()
{
	return g_hasSnapshot;
}

int DummyRecorder::GetChangeCount()
{
	return g_changeCount;
}

bool DummyRecorder::GetChange(int index, uintptr_t& outRva, uint32_t& outBefore, uint32_t& outAfter,
	int& outLength)
{
	if (index < 0 || index >= g_changeCount)
		return false;

	outRva = g_changes[index].rva;
	outBefore = g_changes[index].before;
	outAfter = g_changes[index].after;
	outLength = g_changes[index].length;
	return true;
}

int DummyRecorder::GetTransitionCount()
{
	return g_historyCount;
}

bool DummyRecorder::GetTransition(int index, uint32_t& outFrom, uint32_t& outTo)
{
	if (index < 0 || index >= g_historyCount)
		return false;

	outFrom = g_history[index].from;
	outTo = g_history[index].to;
	return true;
}
