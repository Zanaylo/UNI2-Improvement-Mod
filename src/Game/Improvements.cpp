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
		"1920x1080. On a 1080p screen that is one back buffer pixel per screen pixel - nothing "
		"supersampled, nothing thrown away, and the overlay stays exactly sharp.",
		1920, 1080,
	},
	{
		"1440p",
		"2560x1440, fitted to your window. Four samples per pixel on the HUD and the menus. Into a "
		"1080p window the ratio is not whole, so the overlay softens a little.",
		2560, 1440,
	},
	{
		"4K",
		"3840x2160. Nine samples per pixel and nine times the fill rate. Into a 1080p window it is "
		"an exact 2:1, which is the cleanest of the three.",
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
