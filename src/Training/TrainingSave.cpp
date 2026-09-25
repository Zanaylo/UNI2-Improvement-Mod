#include "Training/TrainingSave.h"

#include "Core/logger.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/OnlineState.h"
#include "Game/Replays/ReplayState.h"
#include "Game/Engine/SaveData.h"

#include <Windows.h>

namespace {

constexpr DWORD kRetryMs = 5000;
constexpr DWORD kPumpLimitMs = 15000;

bool g_pumping = false;
DWORD g_startedAt = 0;

bool InOfflineTraining()
{
	return GameState::IsTrainingBattle() && GameState::IsInMatch() && !OnlineState::IsOnline()
		&& !ReplayState::IsPlaying();
}

void Begin(DWORD now)
{
	if (now - g_startedAt < kRetryMs || !SaveData::IsPending() || !SaveData::Start())
		return;

	g_pumping = true;
	g_startedAt = now;
	LOG("TrainingSave: the training settings changed, writing the save");
}

void Continue(DWORD now)
{
	SaveData::Pump();

	if (!SaveData::IsBusy())
	{
		g_pumping = false;
		LOG("TrainingSave: the save is written after %lu ms", now - g_startedAt);
		return;
	}

	if (now - g_startedAt < kPumpLimitMs)
		return;

	g_pumping = false;
	LOG("TrainingSave: the save was still busy after %lu ms, it is left to the game", now - g_startedAt);
}

}

void TrainingSave::OnFrame()
{
	if (!InOfflineTraining())
	{
		g_pumping = false;
		return;
	}

	const DWORD now = GetTickCount();

	if (g_pumping)
	{
		Continue(now);
		return;
	}

	Begin(now);
}
