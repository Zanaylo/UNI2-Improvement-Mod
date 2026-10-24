#include "Game/Display/SceneLookLevels.h"

namespace {

const char* const kCharacterNames[SceneLookLevels::kCount] = { "Smooth", "Pixels" };
const char* const kTextNames[SceneLookLevels::kCount] = { "Smooth", "Sharp" };

}

int SceneLookLevels::CharacterLevel(bool smooth)
{
	return smooth ? Character_Smooth : Character_Pixels;
}

bool SceneLookLevels::IsSmoothCharacters(int level)
{
	return level != Character_Pixels;
}

const char* SceneLookLevels::CharacterName(int level)
{
	return kCharacterNames[CharacterLevel(IsSmoothCharacters(level))];
}

int SceneLookLevels::TextLevel(bool sharp)
{
	return sharp ? Text_Sharp : Text_Smooth;
}

bool SceneLookLevels::IsSharpText(int level)
{
	return level == Text_Sharp;
}

const char* SceneLookLevels::TextName(int level)
{
	return kTextNames[TextLevel(IsSharpText(level))];
}
