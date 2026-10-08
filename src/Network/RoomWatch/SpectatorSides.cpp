#include "Network/RoomWatch/SpectatorSides.h"

namespace {

constexpr int kNone = -1;
constexpr int kFirst = 0;
constexpr int kSecond = 1;

}

int SpectatorSides::ViewSide(bool settingsWin, int localSide, int remoteSide)
{
	const int side = settingsWin ? localSide : remoteSide;

	if (side == kNone)
		return kNoSide;

	if (side == kFirst)
		return kFirst;

	return side == kSecond ? kSecond : kNoSide;
}

bool SpectatorSides::IsNotFirstSide(int localSide)
{
	return localSide != kFirst;
}
