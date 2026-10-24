#pragma once

namespace SceneLookLevels
{
	constexpr int kCount = 2;

	enum CharacterLevel
	{
		Character_Smooth = 0,
		Character_Pixels = 1
	};

	enum TextLevel
	{
		Text_Smooth = 0,
		Text_Sharp = 1
	};

	int CharacterLevel(bool smooth);
	bool IsSmoothCharacters(int level);
	const char* CharacterName(int level);

	int TextLevel(bool sharp);
	bool IsSharpText(int level);
	const char* TextName(int level);
}
