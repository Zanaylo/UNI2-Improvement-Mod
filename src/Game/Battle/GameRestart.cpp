#include "Game/Battle/GameRestart.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Audio/BgmControl.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/OnlineState.h"
#include "Game/Engine/SceneWatch.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kLeaveFrames = 900;

char g_status[256] = "";
bool g_pending = false;
int g_waited = 0;

bool Poke(uintptr_t rva, uint32_t value)
{
	const uintptr_t at = RvaToAddress(rva);

	return IsAddressInGameModule(at) && TryWriteDword(reinterpret_cast<void*>(at), value);
}

bool RequestScene(uint32_t scene)
{
	BgmControl::Stop();

	return Poke(GameOffsets::kSceneResultA, 0) &&
		Poke(GameOffsets::kSceneResultB, 0) &&
		Poke(GameOffsets::kSceneId, scene) &&
		Poke(GameOffsets::kSceneRequest, 1) &&
		Poke(GameOffsets::kSceneRequestFlag, 1);
}

bool EnterStart()
{
	const uint32_t scene = SceneWatch::First();

	if (!RequestScene(scene))
	{
		strncpy_s(g_status, "the scene request could not be written", _TRUNCATE);
		LOG("GameRestart: %s", g_status);
		return false;
	}

	sprintf_s(g_status, "back to scene %u, where the game started", scene);
	LOG("GameRestart: %s", g_status);
	return true;
}

}

bool GameRestart::CanSoftReset()
{
	if (SceneWatch::First() == SceneWatch::kNone || g_pending)
		return false;

	return !GameState::IsInMatch() || GameState::IsTrainingBattle();
}

bool GameRestart::SoftReset()
{
	if (SceneWatch::First() == SceneWatch::kNone)
	{
		strncpy_s(g_status, "the mod has not seen the game start yet", _TRUNCATE);
		return false;
	}

	if (OnlineState::IsOnline())
	{
		strncpy_s(g_status, "not during an online match", _TRUNCATE);
		return false;
	}

	if (!GameState::IsInMatch())
		return EnterStart();

	if (!GameState::IsTrainingBattle())
	{
		strncpy_s(g_status, "leave the match first", _TRUNCATE);
		return false;
	}

	if (!RequestScene(GameOffsets::kSceneMenu))
	{
		strncpy_s(g_status, "the scene request could not be written", _TRUNCATE);
		LOG("GameRestart: %s", g_status);
		return false;
	}

	g_pending = true;
	g_waited = kLeaveFrames;

	strncpy_s(g_status, "leaving training, then going back to the start", _TRUNCATE);
	LOG("GameRestart: %s", g_status);
	return true;
}

void GameRestart::OnFrame()
{
	if (!g_pending)
		return;

	if (GameState::IsInMatch())
	{
		if (--g_waited > 0)
			return;

		g_pending = false;

		strncpy_s(g_status, "the match did not end, so nothing was done", _TRUNCATE);
		LOG("GameRestart: %s", g_status);
		return;
	}

	g_pending = false;
	EnterStart();
}

bool GameRestart::IsPending()
{
	return g_pending;
}

const char* GameRestart::StatusText()
{
	return g_status;
}
