// One switch for a machine that cannot hold 60.
//
// It is a preset over levers that already exist and none of them reaches the simulation, so a match
// played on it is the same match. The stage keeps drawing at every level - emptying it is a separate
// switch, because a match with no background is a worse trade than a soft one.

#pragma once

namespace PotatoMode
{
	enum Level
	{
		Level_Off = 0,
		Level_Balanced = 1,
		Level_Potato = 2,
		Level_COUNT
	};

	void Apply(int level);
	void ApplySaved();
	void OnFrame();

	int GetLevel();
	bool IsActive();

	// The Potato level's picture size, as the height of a 16:9 frame. The tab offers these and
	// nothing else, because a size nobody asked for is a size nobody can report a bug about.
	constexpr int kHeights[] = { 480, 360, 240, 144 };

	int ClampHeight(int height);
	void SetHeight(int height);
	int GetHeight();
	void SizeForHeight(int height, int& outWidth, int& outHeight);

	const char* GetLevelName(int level);
	const char* Describe(int level);
}
