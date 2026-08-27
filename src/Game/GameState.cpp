#include "Game/GameState.h"

#include "Core/utils.h"
#include "Game/CharaTracker.h"
#include "Game/GameOffsets.h"
#include "Game/MemoryMap.h"
#include "Game/OnlineState.h"

#include <Windows.h>

namespace {

constexpr int kMinEntitiesForMatch = 2;
constexpr DWORD kMatchCacheMs = 200;

bool g_inMatch = false;
DWORD g_lastCheckTick = 0;

constexpr DWORD kTickStaleMs = 250;

bool g_hasTickSample = false;
uint32_t g_lastTickCounter = 0;
DWORD g_lastTickChangeTick = 0;

}

int GameState::GetBattleMode()
{
	uint32_t value = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kBattleMode)), value))
		return -1;

	return static_cast<int>(value);
}

int GameState::GetTrainingFlag()
{
	uint32_t value = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kSubMode)), value))
		return -1;

	return static_cast<int>(value);
}

bool GameState::IsTrainingBattle()
{
	const int mode = GetBattleMode();

	if (mode == 0)
		return GetTrainingFlag() == 1;

	return mode == 3 || mode == 4 || mode == 5;
}

bool GameState::IsSingleMode()
{
	const int mode = GetBattleMode();
	const int sub = GetTrainingFlag();

	if (mode == 0)
		return sub == 0 || sub == 2 || sub == 3 || sub == 4;

	return mode == 5;
}

bool GameState::IsBattleTicking()
{
	uint32_t counter = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kFrameCounterA)), counter))
		return false;

	const DWORD now = GetTickCount();

	if (!g_hasTickSample || counter != g_lastTickCounter)
	{
		g_hasTickSample = true;
		g_lastTickCounter = counter;
		g_lastTickChangeTick = now;
		return true;
	}

	return now - g_lastTickChangeTick < kTickStaleMs;
}

bool GameState::AllowsTrainingTools()
{
	return IsInMatch() && !OnlineState::IsOnline();
}

bool GameState::AllowsPalettes()
{
	return IsInMatch();
}

bool GameState::IsSimulating()
{
	return CharaTracker::IsBattleActive();
}

bool GameState::IsInMatch()
{
	const DWORD now = GetTickCount();
	if (now - g_lastCheckTick < kMatchCacheMs)
		return g_inMatch;

	g_lastCheckTick = now;
	g_inMatch = false;

	MemoryMap::CharaStackView stack = {};
	if (!MemoryMap::ReadCharaStack(stack) || stack.basePointer == 0)
		return false;

	void* entities[16] = {};
	CharaTracker::Entry entry = {};

	for (int i = 0; i < CharaTracker::GetEntryCount(); ++i)
	{
		if (!CharaTracker::GetEntry(i, entry) || !MemoryMap::IsPlayerData(entry.charaData))
			continue;

		const int count = MemoryMap::EnumeratePlayerData(entry.charaData, entities, 16);
		if (count >= kMinEntitiesForMatch)
		{
			g_inMatch = true;
			break;
		}
	}

	return g_inMatch;
}

int GameState::GetLoadedCharacter(int side)
{
	if (side < 0 || side > 1)
		return -1;

	const uintptr_t address = RvaToAddress(GameOffsets::kCharaRecordBase) +
		side * GameOffsets::kCharaRecordStride;

	uint32_t number = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(address), number))
		return -1;

	if (number > 0x7fffffffu)
		return -1;

	return static_cast<int>(number);
}
