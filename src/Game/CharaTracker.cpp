#include "Game/CharaTracker.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <MinHook.h>
#include <Windows.h>

extern "C" __declspec(dllimport) USHORT WINAPI RtlCaptureStackBackTrace(
	ULONG FramesToSkip, ULONG FramesToCapture, PVOID* BackTrace, PULONG BackTraceHash);

namespace {

constexpr uintptr_t kStackEntryStride = 8;
constexpr uint32_t kBattleIdleTimeoutMs = 250;

using GetPP_t = int(__cdecl*)(int);
using PlayerDataDtor_t = void(__fastcall*)(void*, void*);

GetPP_t oGetPP = nullptr;
PlayerDataDtor_t oPlayerDataDtor = nullptr;

bool g_installed = false;
volatile uint64_t g_callCount = 0;
volatile uint32_t g_lastCallTick = 0;

constexpr int kMaxStackFrames = 24;
volatile long g_captureRequested = 0;
uintptr_t g_stackFrames[kMaxStackFrames] = {};
int g_stackFrameCount = 0;
volatile long g_entryCount = 0;

CharaTracker::Entry g_entries[CharaTracker::kMaxEntries] = {};

uintptr_t g_stackBaseGlobal = 0;
uintptr_t g_stackTopGlobal = 0;

bool LooksLikePointer(uintptr_t value)
{
	return value >= 0x10000 && (value & 3) == 0;
}

bool ReadCurrentEntity(void*& outObject, void*& outCharacter)
{
	bool ok = false;

	__try
	{
		const uintptr_t base = *reinterpret_cast<uintptr_t*>(g_stackBaseGlobal);
		const uintptr_t top = *reinterpret_cast<uintptr_t*>(g_stackTopGlobal);

		if (!LooksLikePointer(base) || !LooksLikePointer(top) || top < base + kStackEntryStride)
			return false;

		const uintptr_t slot = top - kStackEntryStride;
		const uintptr_t object = *reinterpret_cast<uintptr_t*>(slot);
		if (!LooksLikePointer(object))
			return false;

		const uintptr_t owner = *reinterpret_cast<uintptr_t*>(object + GameOffsets::kCharaOwner);

		outObject = reinterpret_cast<void*>(object);
		outCharacter = reinterpret_cast<void*>(LooksLikePointer(owner) ? owner : object);
		ok = true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ok = false;
	}

	return ok;
}

void* ReadParamBlock(void* charaData)
{
	void* result = nullptr;

	__try
	{
		const uintptr_t value = *reinterpret_cast<uintptr_t*>(
			reinterpret_cast<uintptr_t>(charaData) + GameOffsets::kCharaParamBlock);

		if (LooksLikePointer(value))
			result = reinterpret_cast<void*>(value);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		result = nullptr;
	}

	return result;
}

void Forget(void* charaData)
{

	for (long i = 0; i < g_entryCount && i < CharaTracker::kMaxEntries;)
	{
		if (g_entries[i].object != charaData && g_entries[i].charaData != charaData)
		{
			++i;
			continue;
		}

		const long last = g_entryCount - 1;
		if (i != last)
			g_entries[i] = g_entries[last];

		memset(&g_entries[last], 0, sizeof(g_entries[last]));
		InterlockedDecrement(&g_entryCount);
	}
}

void __fastcall HookedPlayerDataDtor(void* thisPtr, void* unused)
{
	Forget(thisPtr);
	oPlayerDataDtor(thisPtr, unused);
}

long g_lastSlot = 0;

void Touch(long slot, void* charaData, uint32_t now)
{
	++g_entries[slot].hits;
	g_entries[slot].charaData = charaData;
	g_entries[slot].lastSeenTick = now;
	g_lastSlot = slot;
}

void Record(void* object, void* charaData, uint32_t now)
{
	const long count = g_entryCount;

	if (g_lastSlot < count && g_lastSlot < CharaTracker::kMaxEntries &&
		g_entries[g_lastSlot].object == object)
	{
		Touch(g_lastSlot, charaData, now);
		return;
	}

	for (long i = 0; i < count && i < CharaTracker::kMaxEntries; ++i)
	{
		if (g_entries[i].object == object)
		{
			Touch(i, charaData, now);
			return;
		}
	}

	long slot = count;

	if (count >= CharaTracker::kMaxEntries)
	{
		slot = 0;
		for (long i = 1; i < CharaTracker::kMaxEntries; ++i)
		{
			if (g_entries[i].lastSeenTick < g_entries[slot].lastSeenTick)
				slot = i;
		}
	}

	g_entries[slot].object = object;
	g_entries[slot].charaData = charaData;
	g_entries[slot].paramBlock = ReadParamBlock(charaData);
	g_entries[slot].hits = 1;
	g_entries[slot].lastSeenTick = now;
	g_lastSlot = slot;

	if (slot == count)
		InterlockedIncrement(&g_entryCount);
}

int __cdecl HookedGetPP(int index)
{
	++g_callCount;

	const uint32_t now = GetTickCount();
	g_lastCallTick = now;

	if (g_captureRequested != 0 && InterlockedCompareExchange(&g_captureRequested, 0, 1) == 1)
	{
		void* frames[kMaxStackFrames] = {};
		const USHORT captured = RtlCaptureStackBackTrace(0, kMaxStackFrames, frames, nullptr);
		const uintptr_t base = GetGameBaseAddress();

		g_stackFrameCount = 0;
		for (USHORT i = 0; i < captured && g_stackFrameCount < kMaxStackFrames; ++i)
		{
			const uintptr_t address = reinterpret_cast<uintptr_t>(frames[i]);
			if (base == 0 || address < base)
				continue;

			const uintptr_t rva = address - base;
			g_stackFrames[g_stackFrameCount++] = rva;
			LOG("CharaTracker: script stack [%d] rva 0x%06x", g_stackFrameCount - 1, (unsigned)rva);
		}
	}

	void* object = nullptr;
	void* character = nullptr;
	if (ReadCurrentEntity(object, character))
		Record(object, character, now);

	return oGetPP(index);
}

}

bool CharaTracker::Install()
{
	if (g_installed)
		return true;

	g_stackBaseGlobal = RvaToAddress(GameOffsets::kCharaStackBase);
	g_stackTopGlobal = RvaToAddress(GameOffsets::kCharaStackTop);

	if (g_stackBaseGlobal == 0 || g_stackTopGlobal == 0)
		return false;

	void* target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnGetPP));

	if (!HookManager::CreateAndEnableHook(target, &HookedGetPP, reinterpret_cast<void**>(&oGetPP),
		"GetPP"))
	{
		return false;
	}

	void* dtor = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnPlayerDataDestructor));
	HookManager::CreateAndEnableHook(dtor, &HookedPlayerDataDtor,
		reinterpret_cast<void**>(&oPlayerDataDtor), "PLAYER_DATA::~PLAYER_DATA");

	g_installed = true;
	LOG("CharaTracker installed on GetPP at 0x%p", target);
	return true;
}

bool CharaTracker::IsInstalled()
{
	return g_installed;
}

uint64_t CharaTracker::GetCallCount()
{
	return g_callCount;
}

uint32_t CharaTracker::GetLastCallTick()
{
	return g_lastCallTick;
}

bool CharaTracker::IsBattleActive()
{
	if (g_lastCallTick == 0)
		return false;

	return GetTickCount() - g_lastCallTick < kBattleIdleTimeoutMs;
}

int CharaTracker::GetEntryCount()
{
	return static_cast<int>(g_entryCount);
}

bool CharaTracker::GetEntry(int index, Entry& out)
{
	if (index < 0 || index >= static_cast<int>(g_entryCount) || index >= kMaxEntries)
		return false;

	out = g_entries[index];
	return true;
}

bool CharaTracker::ReadCurrentCharacter(void*& outObject, void*& outCharacter)
{
	outObject = nullptr;
	outCharacter = nullptr;

	if (g_stackBaseGlobal == 0 || g_stackTopGlobal == 0)
		return false;

	return ReadCurrentEntity(outObject, outCharacter);
}

void CharaTracker::RequestStackCapture()
{
	g_stackFrameCount = 0;
	InterlockedExchange(&g_captureRequested, 1);
}

int CharaTracker::GetStackFrameCount()
{
	return g_stackFrameCount;
}

uintptr_t CharaTracker::GetStackFrame(int index)
{
	if (index < 0 || index >= g_stackFrameCount)
		return 0;

	return g_stackFrames[index];
}

bool CharaTracker::IsFresh(const Entry& entry, uint32_t maxAgeMs)
{
	if (entry.object == nullptr)
		return false;

	return GetTickCount() - entry.lastSeenTick <= maxAgeMs;
}

void CharaTracker::Clear()
{
	g_entryCount = 0;
	g_callCount = 0;
	g_lastCallTick = 0;
	g_lastSlot = 0;
	memset(g_entries, 0, sizeof(g_entries));
}
