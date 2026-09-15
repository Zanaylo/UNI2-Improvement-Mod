#include "Game/Improvements.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Game/PresentSize.h"

namespace {

struct Step
{
	const char* name;
	const char* description;
	int width;
	int height;
};

constexpr Step kSteps[Improvements::Level_COUNT] = {
	{
		"Off",
		"The game's own Display option, untouched.",
		0, 0,
	},
	{
		"1080p",
		"1920x1080. On a 1080p screen each pixel lands on one screen pixel, so nothing is lost "
		"and the overlay stays sharp.",
		1920, 1080,
	},
	{
		"1440p",
		"2560x1440, scaled to your window. Four samples per pixel on the HUD and menus. On a "
		"1080p window the scale is not a whole number, so the overlay gets a little soft.",
		2560, 1440,
	},
	{
		"4K",
		"3840x2160. Nine samples per pixel and nine times the drawing work. On a 1080p window it "
		"scales exactly 2:1, the cleanest of the three.",
		3840, 2160,
	},
};

int ClampLevel(int level)
{
	if (level < Improvements::Level_Off)
		return Improvements::Level_Off;

	if (level >= Improvements::Level_COUNT)
		return Improvements::Level_COUNT - 1;

	return level;
}

}

void Improvements::Apply(int level)
{
	g_modVals.supersample = ClampLevel(level);

	Settings::SaveInt("Graphics", "Supersample", g_modVals.supersample);
	PresentSize::Refresh();

	LOG("improvements %s", kSteps[g_modVals.supersample].name);
}

int Improvements::GetLevel()
{
	return ClampLevel(g_modVals.supersample);
}

bool Improvements::GetPresentSize(int& outWidth, int& outHeight)
{
	const Step& step = kSteps[GetLevel()];

	outWidth = step.width;
	outHeight = step.height;

	return step.width > 0 && step.height > 0;
}

const char* Improvements::GetLevelName(int level)
{
	return kSteps[ClampLevel(level)].name;
}

const char* Improvements::Describe(int level)
{
	return kSteps[ClampLevel(level)].description;
}
