#include "Game/PotatoMode.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "D3D9/DeviceHooks.h"
#include "D3D9/SceneScale.h"
#include "Game/EngineQuality.h"
#include "Game/PresentSize.h"
#include "Game/PumpWait.h"

namespace {

struct Preset
{
	const char* name;
	const char* description;
	int presentWidth;
	int presentHeight;
	bool sizeFromPotatoHeight;
	bool disableBackBufferAa;
	bool pumpWait;
	bool disableCharacterFilter;
};

constexpr Preset kPresets[PotatoMode::Level_COUNT] = {
	{
		"Off",
		"Everything as the game ships it, at whatever its own Display option asks for.",
		0, 0, false, false, false, false,
	},
	{
		"Balanced",
		"Draws at 960x540 and stretches that up, drops the back buffer's anti-aliasing - which a "
		"Direct3D 9 texture cannot use anyway - and waits on the frame handshake instead of on the "
		"clock. In exclusive fullscreen the scene targets carry the size instead, because a back "
		"buffer there can only be a size the monitor lists. Slightly soft.",
		960, 540, false, true, true, false,
	},
	{
		"Potato",
		"Draws at the size chosen below and stretches that up, whatever the window or the monitor "
		"is, and turns off Character Visual Improvements: nine palette lookups per character pixel "
		"for a blur one screen pixel wide. In exclusive fullscreen the scene targets carry the size, "
		"since a back buffer there can only be a size the monitor lists. Visibly soft, and the "
		"stage still draws.",
		0, 0, true, true, true, true,
	},
};

struct Snapshot
{
	bool disableBackBufferAa;
	bool pumpWait;
	bool disableCharacterFilter;
	bool taken;
};

Snapshot g_before = {};

bool g_scaleHeld = false;
int g_scaleUser = 100;
int g_scaleWanted = 0;

int ScenePercentFor(int level)
{
	if (level == PotatoMode::Level_Balanced)
		return 75;

	if (level != PotatoMode::Level_Potato)
		return 100;

	const int percent = PotatoMode::GetHeight() * 100 / 720;

	return percent < 20 ? 20 : percent;
}

bool ExclusiveFullscreen()
{
	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();

	return present.BackBufferWidth != 0 && present.Windowed == FALSE;
}

void FollowDisplayMode()
{
	const bool wanted = PotatoMode::IsActive() && ExclusiveFullscreen();
	const int percent = wanted ? ScenePercentFor(PotatoMode::GetLevel()) : 0;

	if (wanted == g_scaleHeld && percent == g_scaleWanted)
		return;

	if (wanted)
	{
		if (!g_scaleHeld)
			g_scaleUser = g_modVals.sceneScalePercent;

		g_modVals.sceneScalePercent = percent;
	}
	else
	{
		g_modVals.sceneScalePercent = g_scaleUser;
	}

	g_scaleHeld = wanted;
	g_scaleWanted = percent;

	LOG("[PotatoMode] %s display, scene scale %d%%",
		wanted ? "exclusive fullscreen" : "windowed or off", g_modVals.sceneScalePercent);

	SceneScale::Apply();
}

int ClampLevel(int level)
{
	if (level < PotatoMode::Level_Off)
		return PotatoMode::Level_Off;

	if (level >= PotatoMode::Level_COUNT)
		return PotatoMode::Level_COUNT - 1;

	return level;
}

void TakeSnapshot()
{
	if (g_before.taken)
		return;

	g_before.disableBackBufferAa = g_modVals.disableBackBufferAa;
	g_before.pumpWait = g_modVals.pumpWait;
	g_before.disableCharacterFilter = g_modVals.disableCharacterFilter;
	g_before.taken = true;
}

void Restore(const Snapshot& snapshot)
{
	g_modVals.disableBackBufferAa = snapshot.disableBackBufferAa;
	g_modVals.pumpWait = snapshot.pumpWait;
	g_modVals.disableCharacterFilter = snapshot.disableCharacterFilter;
}

void RestoreOrShip()
{
	const bool taken = g_before.taken;
	g_before.taken = false;

	if (taken)
	{
		Restore(g_before);
		return;
	}

	const Preset& off = kPresets[PotatoMode::Level_Off];

	g_modVals.disableBackBufferAa = off.disableBackBufferAa;
	g_modVals.pumpWait = off.pumpWait;
	g_modVals.disableCharacterFilter = off.disableCharacterFilter;
}

void Adopt(const Preset& preset)
{
	g_modVals.disableBackBufferAa = preset.disableBackBufferAa;
	g_modVals.pumpWait = preset.pumpWait;
	g_modVals.disableCharacterFilter = preset.disableCharacterFilter;
}

void Save()
{
	Settings::SaveInt("Graphics", "PotatoMode", g_modVals.potatoMode);
	Settings::SaveInt("Graphics", "PotatoHeight", g_modVals.potatoHeight);
	Settings::SaveInt("Graphics", "PresentWidth", g_modVals.presentWidth);
	Settings::SaveInt("Graphics", "PresentHeight", g_modVals.presentHeight);
	Settings::SaveInt("Graphics", "DisableBackBufferAA", g_modVals.disableBackBufferAa ? 1 : 0);
	Settings::SaveInt("Graphics", "DisableCharacterFilter",
		g_modVals.disableCharacterFilter ? 1 : 0);
	Settings::SaveInt("Video", "PumpWait", g_modVals.pumpWait ? 1 : 0);
}

}

void PotatoMode::Apply(int level)
{
	const int clamped = ClampLevel(level);

	if (clamped == Level_Off)
	{
		RestoreOrShip();
	}
	else
	{
		TakeSnapshot();
		Adopt(kPresets[clamped]);
	}

	g_modVals.potatoMode = clamped;

	PresentSize::Refresh();
	Save();

	PumpWait::Apply();
	EngineQuality::Apply();

	LOG("potato mode %s", kPresets[clamped].name);
}


void PotatoMode::ApplySaved()
{
	const int level = GetLevel();

	if (level != Level_Off)
		Adopt(kPresets[level]);

	PresentSize::Refresh();
}

void PotatoMode::OnFrame()
{
	EngineQuality::OnFrame();
	FollowDisplayMode();
}

int PotatoMode::ClampHeight(int height)
{
	for (const int candidate : kHeights)
	{
		if (candidate == height)
			return height;
	}

	return 360;
}

void PotatoMode::SizeForHeight(int height, int& outWidth, int& outHeight)
{
	outHeight = ClampHeight(height);
	outWidth = (outHeight * 16 / 9) & ~1;
}

int PotatoMode::GetHeight()
{
	return ClampHeight(g_modVals.potatoHeight);
}

void PotatoMode::SetHeight(int height)
{
	g_modVals.potatoHeight = ClampHeight(height);

	if (GetLevel() != Level_Potato)
	{
		Settings::SaveInt("Graphics", "PotatoHeight", g_modVals.potatoHeight);
		return;
	}

	Apply(Level_Potato);
}

bool PotatoMode::GetPresentSize(int& outWidth, int& outHeight)
{
	const Preset& preset = kPresets[GetLevel()];

	if (preset.sizeFromPotatoHeight)
	{
		SizeForHeight(GetHeight(), outWidth, outHeight);
		return true;
	}

	outWidth = preset.presentWidth;
	outHeight = preset.presentHeight;

	return preset.presentWidth > 0 && preset.presentHeight > 0;
}

int PotatoMode::GetLevel()
{
	return ClampLevel(g_modVals.potatoMode);
}

bool PotatoMode::IsActive()
{
	return GetLevel() != Level_Off;
}

const char* PotatoMode::GetLevelName(int level)
{
	return kPresets[ClampLevel(level)].name;
}

const char* PotatoMode::Describe(int level)
{
	return kPresets[ClampLevel(level)].description;
}
