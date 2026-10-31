#include "Game/Replays/ReplayNames.h"

namespace {

const char* const kPlaceholders[] = { "P1", "P2", "[unknown]" };
const char* const kSides[] = { "P1", "P2" };

}

bool ReplayNames::IsPlaceholder(const std::string& name)
{
	if (name.empty())
		return true;

	for (const char* const placeholder : kPlaceholders)
	{
		if (name == placeholder)
			return true;
	}

	return false;
}

std::string ReplayNames::Pick(const std::string& player, const std::string& character, int side)
{
	if (!IsPlaceholder(player))
		return player;

	if (!character.empty())
		return character;

	return kSides[side == 0 ? 0 : 1];
}
