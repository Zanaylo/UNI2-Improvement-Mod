#include "Core/Boot/Standby.h"

#include "Core/Boot/StandbyRule.h"
#include "Core/Config/interfaces.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Display/GraphicsOff.h"
#include "Web/UpdateCheck.h"

#include <Windows.h>

namespace {

constexpr unsigned long long kRecheckMs = 15ull * 60ull * 1000ull;

bool g_on = false;
GameBuild::Age g_age = GameBuild::Age::Supported;
unsigned long long g_lastCheck = 0;

void TurnTheModOff()
{
	GraphicsOff::Apply(g_modVals);
	g_modVals.displayTuning = false;
}

const char* Describe(GameBuild::Age age)
{
	return age == GameBuild::Age::Older ? "older" : "newer";
}

}

void Standby::Decide()
{
	g_age = GetGameBuildAge();
	g_on = StandbyRule::Applies(g_age, g_modVals.allowUnsupportedGame);

	if (g_age != GameBuild::Age::Supported && !g_on)
	{
		LOG("Standby: this game build (stamp 0x%08x) is %s than %s, run anyway because [Compat] "
			"AllowUnsupportedGame is on", GetGameBuildStamp(), Describe(g_age), UNI2_IM_SUPPORTED_GAME_VERSION);
		return;
	}

	if (!g_on)
		return;

	TurnTheModOff();
	LOG("Standby: this game build (stamp 0x%08x) is %s than %s, so the mod stays off and only looks for its "
		"next update", GetGameBuildStamp(), Describe(g_age), UNI2_IM_SUPPORTED_GAME_VERSION);
}

bool Standby::IsOn()
{
	return g_on;
}

GameBuild::Age Standby::GameAge()
{
	return g_age;
}

void Standby::OnFrame()
{
	const unsigned long long now = GetTickCount64();

	if (!g_on || UpdateCheck::IsChecking() || !StandbyRule::RecheckDue(now, g_lastCheck, kRecheckMs))
		return;

	g_lastCheck = now;
	UpdateCheck::Refresh();
}
