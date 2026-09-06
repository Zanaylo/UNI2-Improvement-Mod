#include "Overlay/HealthReadout.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/utils.h"
#include "D3D9/GameFont.h"
#include "D3D9/QuadRenderer.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Game/OnlineState.h"
#include "Game/PlayerHealth.h"

#include <cstdint>
#include <cstdio>

namespace {

constexpr float kBarOuter = 0.1121f;
constexpr float kBarTop = 0.0852f;
constexpr float kBarBottom = 0.1074f;

constexpr float kFillHeight = 0.92f;
constexpr float kInset = 0.55f;

constexpr uint32_t kText = 0xFFFFFFFF;
constexpr uint32_t kShadow = 0xF0000000;

bool g_assetsTried = false;

void EnsureAssets(IDirect3DDevice9* device)
{
	if (g_assetsTried)
		return;

	g_assetsTried = true;
	GameFont::Load(device, GetModAssetPath());
}

float Requested()
{
	const int percent = g_modVals.healthValuesScale;

	if (percent < 25)
		return 0.25f;

	if (percent > 400)
		return 4.0f;

	return percent / 100.0f;
}

void DrawShadowed(const char* text, float x, float y, float scale, float thickness, uint32_t colour)
{
	GameFont::Draw(text, x - thickness, y, scale, kShadow);
	GameFont::Draw(text, x + thickness, y, scale, kShadow);
	GameFont::Draw(text, x, y - thickness, scale, kShadow);
	GameFont::Draw(text, x, y + thickness, scale, kShadow);
	GameFont::Draw(text, x, y, scale, colour);
}

void DrawSide(int player, float width, float barHeight, float scale, float y)
{
	PlayerHealth::Health health = {};

	if (!PlayerHealth::Read(MemoryMap::GetCharaSlot(player), health))
		return;

	char line[64] = {};
	sprintf_s(line, "%d / %d", health.current, health.max);

	const float inset = barHeight * kInset;
	const float nudge = static_cast<float>(g_modVals.healthValuesX);
	const float thickness = barHeight * 0.06f;

	if (player == 0)
	{
		DrawShadowed(line, width * kBarOuter + inset + nudge, y, scale, thickness, kText);
		return;
	}

	const float right = width * (1.0f - kBarOuter) - inset - nudge;

	DrawShadowed(line, right - GameFont::MeasureWidth(line, scale), y, scale, thickness, kText);
}

}

bool HealthReadout::IsVisible()
{
	return g_modVals.showHealthValues != 0;
}

void HealthReadout::SetVisible(bool visible)
{
	g_modVals.showHealthValues = visible ? 1 : 0;
	Settings::SaveInt("Training", "ShowHealthValues", g_modVals.showHealthValues);
}

void HealthReadout::Render(IDirect3DDevice9* device)
{
	if (!IsVisible() || device == nullptr)
		return;

	if (OnlineState::IsOnline() || !GameState::IsInMatch())
		return;

	EnsureAssets(device);

	if (!GameFont::IsLoaded())
		return;

	const float lineHeight = GameFont::GetLineHeight();

	if (lineHeight <= 0.0f)
		return;

	D3DVIEWPORT9 viewport = {};

	if (FAILED(device->GetViewport(&viewport)))
		return;

	const float width = static_cast<float>(viewport.Width);
	const float height = static_cast<float>(viewport.Height);
	const float barHeight = (kBarBottom - kBarTop) * height;
	const float target = barHeight * kFillHeight * Requested();
	const float scale = target / lineHeight;

	const float y = kBarTop * height + (barHeight - target) * 0.5f +
		static_cast<float>(g_modVals.healthValuesY);

	if (!QuadRenderer::Begin(device))
		return;

	DrawSide(0, width, barHeight, scale, y);
	DrawSide(1, width, barHeight, scale, y);

	QuadRenderer::End();
}
