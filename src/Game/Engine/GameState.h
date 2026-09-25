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

	int GetLoadedCharacter(int side);
}
