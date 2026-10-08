#pragma once

namespace SpectatorSides
{
	constexpr int kNoSide = 2;

	int ViewSide(bool settingsWin, int localSide, int remoteSide);
	bool IsNotFirstSide(int localSide);
}
