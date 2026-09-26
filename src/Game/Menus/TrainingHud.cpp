#include "Game/Menus/TrainingHud.h"

#include "Core/logger.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/GameHook.h"

#include <cstdio>

namespace {

typedef void(__fastcall* DrawFn)(void* self, void* unused);

GameHook<DrawFn> g_drawHook("TrainingDamageInfoDraw");
TrainingHud::IDrawer* g_drawer = nullptr;

char g_status[128] = "the training HUD is not where this game version expects it";

void __fastcall HookedDraw(void* self, void* unused)
{
	g_drawHook.Original()(self, unused);
	g_drawer->Draw(GameOffsets::kTrainingInfoLayer);
}

}

bool TrainingHud::Install(IDrawer* drawer)
{
	if (drawer == nullptr)
		return false;

	g_drawer = drawer;

	if (!g_drawHook.InstallRva(GameOffsets::kFnTrainingDamageInfoDraw, &HookedDraw))
	{
		LOG("TrainingHud: %s", g_status);
		return false;
	}

	sprintf_s(g_status, "hooked");
	LOG("TrainingHud: hooked");
	return true;
}

const char* TrainingHud::StatusText()
{
	return g_status;
}
