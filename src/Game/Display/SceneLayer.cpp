#include "Game/Display/SceneLayer.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"

namespace {

const char* const kNames[SceneLayer::Level_COUNT] = { "Off", "On" };

int Clamp(int level)
{
	return level == SceneLayer::Level_RenderResolution ? level : SceneLayer::Level_Native;
}

}

int SceneLayer::GetLevel()
{
	return g_modVals.spritesAtRenderResolution ? Level_RenderResolution : Level_Native;
}

void SceneLayer::Apply(int level)
{
	const int clamped = Clamp(level);

	g_modVals.spritesAtRenderResolution = clamped == Level_RenderResolution;
	Settings::SaveInt("Graphics", "SpritesAtRenderResolution", clamped);
	LOG("characters and text filters %s, applies at the next start", kNames[clamped]);
}

const char* SceneLayer::GetLevelName(int level)
{
	return kNames[Clamp(level)];
}
