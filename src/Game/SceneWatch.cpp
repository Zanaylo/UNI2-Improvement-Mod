#include "Game/SceneWatch.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <cstdio>

namespace {

constexpr unsigned kSettleFrames = 4;
constexpr int kMaxLogged = 48;

uint32_t g_raw = SceneWatch::kNone;
uint32_t g_settled = SceneWatch::kNone;
uint32_t g_candidate = SceneWatch::kNone;
unsigned g_held = 0;
int g_logged = 0;
char g_status[96] = "not read yet";

}

void SceneWatch::OnFrame()
{
	uint32_t scene = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kSceneId)), scene))
		return;

	g_raw = scene;

	if (scene != g_candidate)
	{
		g_candidate = scene;
		g_held = 0;
		return;
	}

	if (g_held < kSettleFrames)
		++g_held;

	if (g_held < kSettleFrames || scene == g_settled)
		return;

	const uint32_t previous = g_settled;
	g_settled = scene;

	sprintf_s(g_status, "scene %u", scene);

	if (g_logged >= kMaxLogged)
		return;

	++g_logged;
	LOG("SceneWatch: scene %u, was %u", scene, previous);
}

uint32_t SceneWatch::Current()
{
	return g_settled;
}

uint32_t SceneWatch::Raw()
{
	return g_raw;
}

unsigned SceneWatch::HeldFrames()
{
	return g_held;
}

const char* SceneWatch::StatusText()
{
	return g_status;
}
