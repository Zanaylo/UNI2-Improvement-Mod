#include "Overlay/Hud/GrdPopupHud.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "D3D9/Device/DeviceHooks.h"
#include "D3D9/Draw/GameFont.h"
#include "D3D9/Draw/QuadRenderer.h"
#include "Training/Meter/GrdWatch.h"

#include <cstdint>
#include <cstdio>

namespace {

constexpr float kSideCentreX[GrdWatch::kPlayers] = { 0.435f, 0.565f };
constexpr float kAnchorY = 0.905f;
constexpr float kTextScale = 1.2f;
constexpr float kRisePerFrame = 0.0011f;
constexpr float kOutline = 0.06f;
constexpr int kFadeFrames = 20;

constexpr float kTimerCentreX = 0.5f;
constexpr float kTimerCentreY = 0.87f;
constexpr int kFramesPerSecond = 60;
constexpr int kTimerWarnFrames = 2 * kFramesPerSecond;

constexpr int kHundredth = GrdWatch::kUnitsPerBlock / 100;

constexpr uint32_t kGain = 0x0078F090;
constexpr uint32_t kLoss = 0x00FF6464;
constexpr uint32_t kZero = 0x00A8A8A8;
constexpr uint32_t kTimer = 0x00FFFFFF;
constexpr uint32_t kTimerWarn = 0x00FFB43C;
constexpr uint32_t kOutlineColour = 0x00000000;


uint32_t WithAlpha(uint32_t rgb, int age, uint32_t maxAlpha)
{
	const int left = GrdWatch::kLifeFrames - age;
	const uint32_t alpha = left >= kFadeFrames
		? maxAlpha : maxAlpha * static_cast<uint32_t>(left) / kFadeFrames;

	return (alpha << 24) | rgb;
}

int RoundToHundredths(int amount)
{
	const int magnitude = amount < 0 ? -amount : amount;
	const int rounded = (magnitude + kHundredth / 2) / kHundredth;

	return amount < 0 ? -rounded : rounded;
}

uint32_t ColourFor(int hundredths)
{
	if (hundredths == 0)
		return kZero;

	return hundredths > 0 ? kGain : kLoss;
}

void Format(int amount, int hundredths, char* out, size_t size)
{
	const int magnitude = hundredths < 0 ? -hundredths : hundredths;

	sprintf_s(out, size, "%c%d.%02d", amount < 0 ? '-' : '+', magnitude / 100, magnitude % 100);
}

void DrawOutlined(const char* text, float x, float y, float scale, float thickness, int age,
	uint32_t colour)
{
	const uint32_t outline = WithAlpha(kOutlineColour, age, 0xF0);

	GameFont::Draw(text, x - thickness, y, scale, outline);
	GameFont::Draw(text, x + thickness, y, scale, outline);
	GameFont::Draw(text, x, y - thickness, scale, outline);
	GameFont::Draw(text, x, y + thickness, scale, outline);
	GameFont::Draw(text, x, y, scale, WithAlpha(colour, age, 0xFF));
}

void DrawPopup(const GrdWatch::Popup& popup, float width, float height, float scale)
{
	if (popup.side < 0 || popup.side >= GrdWatch::kPlayers)
		return;

	const int hundredths = RoundToHundredths(popup.amount);

	char text[16] = {};
	Format(popup.amount, hundredths, text, sizeof(text));

	const float textHeight = GameFont::GetLineHeight() * scale;
	const float x = kSideCentreX[popup.side] * width - GameFont::MeasureWidth(text, scale) * 0.5f;
	const float y = (kAnchorY - popup.age * kRisePerFrame) * height - textHeight;

	DrawOutlined(text, x, y, scale, textHeight * kOutline, popup.age, ColourFor(hundredths));
}

void DrawTimer(float width, float height, float scale)
{
	int frames = 0;
	if (!GrdWatch::ReadCycleRemaining(frames))
		return;

	char text[16] = {};
	sprintf_s(text, "%d.%d", frames / kFramesPerSecond, frames % kFramesPerSecond * 10 / kFramesPerSecond);

	const float textHeight = GameFont::GetLineHeight() * scale;
	const float x = kTimerCentreX * width - GameFont::MeasureWidth(text, scale) * 0.5f;
	const float y = kTimerCentreY * height - textHeight * 0.5f;

	DrawOutlined(text, x, y, scale, textHeight * kOutline, 0,
		frames <= kTimerWarnFrames ? kTimerWarn : kTimer);
}

}

bool GrdPopupHud::IsVisible()
{
	return g_modVals.grdPopups;
}

void GrdPopupHud::SetVisible(bool visible)
{
	g_modVals.grdPopups = visible;
	Settings::SaveInt("Training", "GrdPopups", visible ? 1 : 0);
}

bool GrdPopupHud::IsTimerVisible()
{
	return g_modVals.grdTimer;
}

void GrdPopupHud::SetTimerVisible(bool visible)
{
	g_modVals.grdTimer = visible;
	Settings::SaveInt("Training", "GrdTimer", visible ? 1 : 0);
}

void GrdPopupHud::Render(IDirect3DDevice9* device)
{
	if (device == nullptr || !GrdWatch::IsAllowedHere())
		return;

	GrdWatch::Popup popups[GrdWatch::kMaxPopups] = {};
	const int count = IsVisible() ? GrdWatch::GetPopups(popups, GrdWatch::kMaxPopups) : 0;
	const bool timer = IsTimerVisible();

	if (count == 0 && !timer)
		return;

	GameFont::Ensure(device);

	if (!GameFont::IsLoaded() || GameFont::GetLineHeight() <= 0.0f)
		return;

	D3DVIEWPORT9 viewport = {};
	if (FAILED(device->GetViewport(&viewport)) || viewport.Width == 0 || viewport.Height == 0)
		return;

	const float width = static_cast<float>(viewport.Width);
	const float height = static_cast<float>(viewport.Height);
	const float scale = kTextScale * DeviceHooks::GetOverlayScale();

	if (timer)
		DrawTimer(width, height, scale);

	for (int i = 0; i < count; ++i)
		DrawPopup(popups[i], width, height, scale);
}
