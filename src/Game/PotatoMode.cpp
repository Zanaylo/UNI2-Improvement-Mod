#include "Game/PotatoMode.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "D3D9/RenderScale.h"
#include "Game/PumpWait.h"

namespace {

constexpr int kPotatoPercent = 50;

struct Snapshot
{
	int internalResolutionPercent;
	bool disableBackBufferAa;
	bool pumpWait;
	bool taken;
};

Snapshot g_before = {};

void Save(const char* key, int value)
{
	Settings::SaveInt("Graphics", key, value);
}

}

void PotatoMode::Apply(bool enabled)
{
	if (enabled)
	{
		if (!g_before.taken)
		{
			g_before.internalResolutionPercent = g_modVals.internalResolutionPercent;
			g_before.disableBackBufferAa = g_modVals.disableBackBufferAa;
			g_before.pumpWait = g_modVals.pumpWait;
			g_before.taken = true;
		}

		g_modVals.internalResolutionPercent = kPotatoPercent;
		g_modVals.disableBackBufferAa = true;
		g_modVals.pumpWait = true;
	}
	else if (g_before.taken)
	{
		g_modVals.internalResolutionPercent = g_before.internalResolutionPercent;
		g_modVals.disableBackBufferAa = g_before.disableBackBufferAa;
		g_modVals.pumpWait = g_before.pumpWait;

		g_before.taken = false;
	}
	else
	{
		// Switched on in the ini, so there is nothing to put back - go to the shipped values rather
		// than refusing, which is what used to leave the box stuck on.
		g_modVals.internalResolutionPercent = 100;
		g_modVals.disableBackBufferAa = false;
	}

	g_modVals.potatoMode = enabled;

	Save("PotatoMode", enabled ? 1 : 0);
	Save("InternalResolutionPercent", g_modVals.internalResolutionPercent);
	Save("DisableBackBufferAA", g_modVals.disableBackBufferAa ? 1 : 0);
	Settings::SaveInt("Video", "PumpWait", g_modVals.pumpWait ? 1 : 0);

	RenderScale::Apply();
	PumpWait::Apply();

	LOG("potato mode %s", enabled ? "on" : "off");
}

bool PotatoMode::IsActive()
{
	return g_modVals.potatoMode;
}

const char* PotatoMode::Describe()
{
	return "Renders at half resolution and scales up, drops the back buffer's unused anti-aliasing, "
		"and leaves the rest of the frame alone. The stage still draws.";
}
