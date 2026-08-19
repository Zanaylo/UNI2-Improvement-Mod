#include "Palette/PlayerSides.h"

#include "Game/GameOffsets.h"
#include "Game/MemoryMap.h"

namespace {

bool ReadX(int player, int& out)
{
	void* const chara = MemoryMap::GetCharaSlot(player);

	if (chara == nullptr)
		return false;

	uint32_t x = 0;
	if (!MemoryMap::ReadStructDword(chara, GameOffsets::kPlayerDataBaseX, x))
		return false;

	out = static_cast<int>(x);
	return true;
}

}

int PlayerSides::ScreenSideOf(int player)
{
	if (player < 0 || player > 1)
		return -1;

	int mine = 0;
	int theirs = 0;

	if (!ReadX(player, mine) || !ReadX(player == 0 ? 1 : 0, theirs))
		return -1;

	if (mine == theirs)
		return -1;

	return mine < theirs ? 0 : 1;
}

int PlayerSides::PlayerOnScreenSide(int side)
{
	if (side < 0 || side > 1)
		return -1;

	const int first = ScreenSideOf(0);

	if (first < 0)
		return -1;

	return first == side ? 0 : 1;
}
