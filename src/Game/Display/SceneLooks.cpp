#include "Game/Display/SceneLooks.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Game/Display/SceneLookLevels.h"

int SceneLooks::GetCharacterLevel()
{
	return SceneLookLevels::CharacterLevel(g_modVals.smoothCharacters != 0);
}

void SceneLooks::ApplyCharacter(int level)
{
	const bool smooth = SceneLookLevels::IsSmoothCharacters(level);

	g_modVals.smoothCharacters = smooth;
	Settings::SaveInt("Graphics", "SmoothCharacters", smooth ? 1 : 0);
	LOG("characters drawn %s", SceneLookLevels::CharacterName(level));
}

int SceneLooks::GetTextLevel()
{
	return SceneLookLevels::TextLevel(g_modVals.sharpText != 0);
}

void SceneLooks::ApplyText(int level)
{
	const bool sharp = SceneLookLevels::IsSharpText(level);

	g_modVals.sharpText = sharp;
	Settings::SaveInt("Graphics", "SharpText", sharp ? 1 : 0);
	LOG("text drawn %s", SceneLookLevels::TextName(level));
}
