// "In a match" and "the simulation is ticking" are different questions: hitstop, super freeze and
// the pause menu all stop scripts without leaving the match.

#pragma once

#include <cstdint>

namespace GameState
{
	int GetBattleMode();
	int GetTrainingFlag();
	bool IsTrainingBattle();

	bool IsInMatch();

	bool IsSingleMode();

	bool IsBattleTicking();

	bool AllowsTrainingTools();

	bool AllowsPalettes();

	bool IsSimulating();
}
