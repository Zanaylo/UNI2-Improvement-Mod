// POTATO MODE the other way round: draw the finished frame larger than the window and let Direct3D
// downsample it. The five 1280x720 scene targets are not touched, so this does not add detail to a
// sprite - what it does is supersample everything drawn straight into the back buffer, which is the
// HUD, the menus, the composite's edges and the mod's own overlay.

#pragma once

namespace Improvements
{
	enum Level
	{
		Level_Off = 0,
		Level_1080p = 1,
		Level_1440p = 2,
		Level_4K = 3,
		Level_COUNT
	};

	void Apply(int level);

	int GetLevel();
	bool GetPresentSize(int& outWidth, int& outHeight);

	const char* GetLevelName(int level);
	const char* Describe(int level);
}
