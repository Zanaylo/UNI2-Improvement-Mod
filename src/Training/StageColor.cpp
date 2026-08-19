#include "Training/StageColor.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

namespace {

bool g_enabled = false;
uint32_t g_rgb = 0x00ff00;

bool g_held = false;
uint32_t g_originalLoaded = 0;

}

void StageColor::SetEnabled(bool enabled)
{
	if (g_enabled == enabled)
		return;

	g_enabled = enabled;

	if (g_enabled)
		return;

	if (!g_held)
		return;

	TryWriteDword(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kBgStageDrawn)),
		g_originalLoaded);

	g_held = false;
	LOG("stage colour: stage drawing restored to %u", g_originalLoaded);
}

bool StageColor::IsEnabled()
{
	return g_enabled;
}

void StageColor::SetColor(uint32_t rgb)
{
	g_rgb = rgb & 0x00ffffffu;
}

uint32_t StageColor::GetColor()
{
	return g_rgb;
}

uint32_t StageColor::GetClearColor()
{

	return 0xff000000u | (g_rgb & 0x00ffffffu);
}

void StageColor::OnFrame()
{
	if (!g_enabled)
		return;

	const uintptr_t flag = RvaToAddress(GameOffsets::kBgStageDrawn);

	uint32_t current = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(flag), current))
		return;

	if (current != 0)
	{
		if (!g_held || g_originalLoaded != current)
			LOG("stage colour: suppressing the stage draw, flag is %u", current);

		g_originalLoaded = current;
		g_held = true;

		TryWriteDword(reinterpret_cast<void*>(flag), 0);
	}
}
