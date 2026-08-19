#include "Overlay/FrameMeterHud.h"

#include "Core/interfaces.h"
#include "Core/Settings.h"
#include "Core/utils.h"
#include "D3D9/GameFont.h"
#include "D3D9/QuadRenderer.h"
#include "Game/GameState.h"
#include "Game/OnlineState.h"
#include "Overlay/WindowManager.h"
#include "Training/FrameStepper.h"
#include "Game/PlayerState.h"
#include "Training/DummyRecorder.h"
#include "Training/FrameMeter.h"

#include <imgui.h>

#include <cstdio>

namespace {

constexpr int kVisibleFrames = 90;

constexpr float kCellWidth = 9.0f;
constexpr float kCellGap = 1.0f;
constexpr float kRowHeight = 18.0f;
constexpr float kRowGap = 4.0f;
constexpr float kTextGap = 3.0f;

constexpr float kAttrRowHeight = 12.0f;
constexpr float kAttrRowGap = 2.0f;
constexpr int kMaxAttrSlices = 3;
constexpr float kAttrSliceEdge = 1.0f;
constexpr uint32_t kAttrSliceEdgeColor = 0xC0000000;

constexpr float kTrackWidth = kVisibleFrames * kCellWidth;

constexpr float kTextScale = 0.62f;
constexpr float kCounterScale = 0.52f;

constexpr uint32_t kEmptyCell = 0xD8121316;
constexpr uint32_t kCellEdge = 0x30FFFFFF;
constexpr uint32_t kText = 0xFFFFFFFF;
constexpr uint32_t kTextShadow = 0xE0000000;
constexpr uint32_t kActionName = 0xFFB0B8C8;
constexpr uint32_t kAdvPlus = 0xFF8FD8B0;
constexpr uint32_t kAdvMinus = 0xFFE09090;
constexpr uint32_t kCounterBg = 0xFF101216;
constexpr uint32_t kCounterEdge = 0xFFFFFFFF;

constexpr float kOwnerStripHeight = 4.0f;

bool g_assetsTried = false;
bool g_visible = false;

bool g_dragging = false;
float g_dragOffsetX = 0.0f;
float g_dragOffsetY = 0.0f;

bool DragMeter(const D3DVIEWPORT9& viewport, float x, float y, float width, float height)
{
	if (ImGui::GetCurrentContext() == nullptr)
		return false;

	const ImGuiIO& io = ImGui::GetIO();

	if (!g_modVals.frameMeterDrag || !WindowManager::GetInstance().IsOverlayActive() ||
		io.WantCaptureMouse)
	{
		g_dragging = false;
		return false;
	}

	if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f)
		return false;

	const float mouseX = io.MousePos.x * (viewport.Width / io.DisplaySize.x);
	const float mouseY = io.MousePos.y * (viewport.Height / io.DisplaySize.y);

	if (!io.MouseDown[0])
	{
		if (g_dragging)
		{
			g_dragging = false;
			Settings::SaveInt("FrameMeter", "PositionX", g_modVals.frameMeterX);
			Settings::SaveInt("FrameMeter", "PositionY", g_modVals.frameMeterY);
		}

		return false;
	}

	if (!g_dragging)
	{
		const bool over = mouseX >= x && mouseX < x + width &&
			mouseY >= y && mouseY < y + height;

		if (!over || !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			return false;

		g_dragging = true;
		g_dragOffsetX = mouseX - x;
		g_dragOffsetY = mouseY - y;
	}

	g_modVals.frameMeterX = static_cast<int>(mouseX - g_dragOffsetX);
	g_modVals.frameMeterY = static_cast<int>(mouseY - g_dragOffsetY);

	if (g_modVals.frameMeterX < 0)
		g_modVals.frameMeterX = 0;
	if (g_modVals.frameMeterY < 0)
		g_modVals.frameMeterY = 0;

	return true;
}

void EnsureAssets(IDirect3DDevice9* device)
{
	if (g_assetsTried)
		return;

	g_assetsTried = true;
	GameFont::Load(device, GetModAssetPath());
}

uint32_t Fade(uint32_t color)
{
	int opacity = g_modVals.frameMeterOpacity;
	if (opacity >= 100)
		return color;
	if (opacity < 0)
		opacity = 0;

	const uint32_t a = ((color >> 24) & 0xff) * static_cast<uint32_t>(opacity) / 100u;
	return (a << 24) | (color & 0x00ffffff);
}

void DrawShadowedText(const char* text, float x, float y, float scale, uint32_t color)
{
	const float offset = scale > 0.7f ? 2.0f : 1.0f;
	GameFont::Draw(text, x + offset, y + offset, scale, Fade(kTextShadow));
	GameFont::Draw(text, x, y, scale, Fade(color));
}

bool TrackSlot(int length, int slot, int& outFrame, bool& outPrevious)
{
	const int lap = length / kVisibleFrames;
	const int position = length % kVisibleFrames;

	if (slot < position)
	{
		outFrame = lap * kVisibleFrames + slot;
		outPrevious = false;
		return true;
	}

	if (lap == 0)
		return false;

	outFrame = (lap - 1) * kVisibleFrames + slot;
	outPrevious = true;
	return true;
}

uint32_t Shade(uint32_t color, float factor)
{
	const uint32_t a = (color >> 24) & 0xff;
	uint32_t r = static_cast<uint32_t>(((color >> 16) & 0xff) * factor);
	uint32_t g = static_cast<uint32_t>(((color >> 8) & 0xff) * factor);
	uint32_t b = static_cast<uint32_t>((color & 0xff) * factor);

	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;

	return Fade((a << 24) | (r << 16) | (g << 8) | b);
}

void DrawCell(float x, float y, float width, float height, uint32_t color)
{
	QuadRenderer::FillRectVertical(x, y, width, height, Shade(color, 1.25f), Shade(color, 0.72f));
}

void DrawTrack(float x, float y, float s, int player, int length)
{
	const float cellW = (kCellWidth - kCellGap) * s;
	const float rowH = kRowHeight * s;

	for (int i = 0; i < kVisibleFrames; ++i)
		DrawCell(x + i * kCellWidth * s, y, cellW, rowH, kEmptyCell);

	for (int slot = 0; slot < kVisibleFrames; ++slot)
	{
		int index = 0;
		bool previousLap = false;
		if (!TrackSlot(length, slot, index, previousLap))
			continue;

		FrameMeter::Frame frame = {};
		if (!FrameMeter::GetFrame(player, index, frame) || frame.state == FrameMeter::State::None)
			continue;

		const float cellX = x + slot * kCellWidth * s;
		const float dim = previousLap ? 0.45f : 1.0f;

		if (frame.projectileActive)
		{
			DrawCell(cellX, y, cellW, rowH,
				Shade(FrameMeter::GetMarkerColor(FrameMeter::Marker_Projectile), dim));
			QuadRenderer::FillRect(cellX, y, cellW, kOwnerStripHeight * s,
				Shade(FrameMeter::GetStateColor(frame.state), dim));
		}
		else
		{
			DrawCell(cellX, y, cellW, rowH, Shade(FrameMeter::GetStateColor(frame.state), dim));
		}

		if (frame.teching)
			DrawCell(cellX, y, cellW, rowH, Shade(FrameMeter::GetMarkerColor(FrameMeter::Marker_Tech), dim));
	}

	QuadRenderer::StrokeRect(x - s, y - s, kTrackWidth * s + s * 2.0f, rowH + s * 2.0f, s, Fade(kCellEdge));
}

void DrawAttributeTrack(float x, float y, float s, int player, int length)
{
	const float cellW = (kCellWidth - kCellGap) * s;
	const float rowH = kAttrRowHeight * s;

	for (int i = 0; i < kVisibleFrames; ++i)
		QuadRenderer::FillRect(x + i * kCellWidth * s, y, cellW, rowH, Fade(kEmptyCell));

	for (int slot = 0; slot < kVisibleFrames; ++slot)
	{
		int index = 0;
		bool previousLap = false;
		if (!TrackSlot(length, slot, index, previousLap))
			continue;

		FrameMeter::Frame frame = {};
		if (!FrameMeter::GetFrame(player, index, frame) || frame.invuln == 0)
			continue;

		int slices = 0;
		for (int m = FrameMeter::kFirstInvulnMarker; m < FrameMeter::Marker_COUNT; ++m)
		{
			if ((frame.invuln & FrameMeter::GetMarkerInvulnBit(static_cast<FrameMeter::Marker>(m))) != 0)
				++slices;
		}

		if (slices == 0)
			continue;

		if (slices > kMaxAttrSlices)
			slices = kMaxAttrSlices;

		const float cellX = x + slot * kCellWidth * s;
		const float dim = previousLap ? 0.45f : 1.0f;
		const float sliceH = rowH / static_cast<float>(slices);

		int drawn = 0;
		for (int m = FrameMeter::kFirstInvulnMarker;
			m < FrameMeter::Marker_COUNT && drawn < slices; ++m)
		{
			const FrameMeter::Marker marker = static_cast<FrameMeter::Marker>(m);
			if ((frame.invuln & FrameMeter::GetMarkerInvulnBit(marker)) == 0)
				continue;

			const float sliceY = y + drawn * sliceH;
			QuadRenderer::FillRect(cellX, sliceY, cellW, sliceH,
				Shade(FrameMeter::GetMarkerColor(marker), dim));

			if (drawn + 1 < slices)
			{
				QuadRenderer::FillRect(cellX, sliceY + sliceH - kAttrSliceEdge * s, cellW,
					kAttrSliceEdge * s, Fade(kAttrSliceEdgeColor));
			}

			++drawn;
		}
	}

	QuadRenderer::StrokeRect(x - s, y - s, kTrackWidth * s + s * 2.0f, rowH + s * 2.0f, s, Fade(kCellEdge));
}

bool RunAt(int length, int player, int slot, int& outEnd, FrameMeter::State& outState,
	bool& outPreviousLap)
{
	int index = 0;
	if (!TrackSlot(length, slot, index, outPreviousLap))
		return false;

	FrameMeter::Frame frame = {};
	if (!FrameMeter::GetFrame(player, index, frame))
		return false;

	outState = frame.state;
	outEnd = slot;

	while (outEnd + 1 < kVisibleFrames)
	{
		int nextIndex = 0;
		bool nextPreviousLap = false;
		if (!TrackSlot(length, outEnd + 1, nextIndex, nextPreviousLap) ||
			nextPreviousLap != outPreviousLap)
		{
			break;
		}

		FrameMeter::Frame next = {};
		if (!FrameMeter::GetFrame(player, nextIndex, next) || next.state != outState)
			break;

		++outEnd;
	}

	return true;
}

void DrawRunCounts(float x, float y, float s, int player, int length)
{
	const float rowH = kRowHeight * s;
	const float scale = kCounterScale * s;
	const float textHeight = GameFont::GetLineHeight() * scale;
	const float cellW = (kCellWidth - kCellGap) * s;

	const int position = length % kVisibleFrames;
	const int newest = position > 0 ? position - 1 : kVisibleFrames - 1;
	const bool counted = FrameMeter::GetTrailingRunLength(player) > 0;

	int slot = 0;

	while (slot < kVisibleFrames)
	{
		int end = 0;
		FrameMeter::State state = FrameMeter::State::None;
		bool previousLap = false;

		if (!RunAt(length, player, slot, end, state, previousLap))
		{
			++slot;
			continue;
		}

		const int start = slot;
		const int run = end - start + 1;
		slot = end + 1;

		if (state == FrameMeter::State::None || state == FrameMeter::State::Idle)
			continue;

		if (counted && start <= newest && newest <= end)
			continue;

		char text[8] = {};
		sprintf_s(text, "%d", run);

		const float inset = s;
		const float textWidth = GameFont::MeasureWidth(text, scale);
		const float runWidth = (run - 1) * kCellWidth * s + cellW;

		if (textWidth + inset > runWidth)
			continue;

		const float right = x + end * kCellWidth * s + cellW;

		DrawShadowedText(text, right - textWidth - inset, y + (rowH - textHeight) * 0.5f, scale,
			previousLap ? 0x80FFFFFF : kText);
	}
}

bool DrawLineTotals(float x, float y, float s, int player, int length)
{
	int blockstun = 0;
	int hitstun = 0;
	int gap = 0;
	int pendingGap = 0;
	bool held = false;

	for (int i = 0; i < length; ++i)
	{
		FrameMeter::Frame frame = {};
		if (!FrameMeter::GetFrame(player, i, frame))
			continue;

		if (frame.state == FrameMeter::State::Blockstun ||
			frame.state == FrameMeter::State::Hitstun)
		{
			if (frame.state == FrameMeter::State::Blockstun)
				++blockstun;
			else
				++hitstun;

			gap += pendingGap;
			pendingGap = 0;
			held = true;
			continue;
		}

		if (held)
			++pendingGap;
	}

	if (blockstun == 0 && hitstun == 0)
		return false;

	char text[96] = {};
	int at = 0;

	if (blockstun > 0)
		at += sprintf_s(text + at, sizeof(text) - at, "blockstun %d", blockstun);

	if (hitstun > 0)
	{
		at += sprintf_s(text + at, sizeof(text) - at, "%shitstun %d",
			at > 0 ? "  " : "", hitstun);
	}

	if (gap > 0)
		at += sprintf_s(text + at, sizeof(text) - at, "%sgap %d", at > 0 ? "  " : "", gap);

	DrawShadowedText(text, x, y, kTextScale * s, kActionName);
	return true;
}

void DrawTrailingCounter(float x, float y, float s, int player, int length)
{
	const int run = FrameMeter::GetTrailingRunLength(player);
	if (run <= 0)
		return;

	const int drawn = length % kVisibleFrames;
	if (drawn <= 0)
		return;

	char text[8] = {};
	sprintf_s(text, "%d", run);

	const float scale = kCounterScale * s;
	const float textWidth = GameFont::MeasureWidth(text, scale);
	const float boxWidth = textWidth + 8.0f * s;
	const float boxHeight = kRowHeight * s;

	float boxX = x + drawn * kCellWidth * s;
	if (boxX + boxWidth > x + kTrackWidth * s)
		boxX = x + kTrackWidth * s - boxWidth;

	QuadRenderer::FillRect(boxX, y, boxWidth, boxHeight, Fade(kCounterBg));
	QuadRenderer::StrokeRect(boxX, y, boxWidth, boxHeight, s, Fade(kCounterEdge));

	GameFont::Draw(text, boxX + (boxWidth - textWidth) * 0.5f,
		y + (boxHeight - GameFont::GetLineHeight() * scale) * 0.5f, scale, Fade(kText));
}

void DrawReadout(float x, float y, float s, int player)
{
	char text[96] = {};

	int startup = 0;
	int active = 0;
	int recovery = 0;
	int total = 0;

	if (FrameMeter::GetLastMove(player, startup, active, recovery, total))
		sprintf_s(text, "Startup %dF / Total %dF / Advantage ", startup, total);
	else
		sprintf_s(text, "Startup -- / Total -- / Advantage ");

	const float scale = kTextScale * s;
	DrawShadowedText(text, x, y, scale, kText);

	const float used = GameFont::MeasureWidth(text, scale);

	char advantage[16] = {};
	uint32_t color = kText;

	int value = 0;
	const bool hasAdvantage = FrameMeter::GetAdvantageFor(player, value);

	if (hasAdvantage)
	{
		sprintf_s(advantage, "%+dF", value);
		color = value >= 0 ? kAdvPlus : kAdvMinus;
	}
	else
	{
		sprintf_s(advantage, "--");
	}

	DrawShadowedText(advantage, x + used, y, scale, color);

	float at = x + used + GameFont::MeasureWidth(advantage, scale);

	int afterRecovery = 0;
	if (hasAdvantage && FrameMeter::GetAdvantageAfterRecoveryFor(player, afterRecovery))
	{
		char bracket[16] = {};
		sprintf_s(bracket, " (%+dF)", afterRecovery);

		DrawShadowedText(bracket, at, y, scale, afterRecovery >= 0 ? kAdvPlus : kAdvMinus);
		at += GameFont::MeasureWidth(bracket, scale);
	}

	const char* const action = FrameMeter::GetActionName(player);

	if (action[0] == '\0')
		return;

	char named[64] = {};
	sprintf_s(named, "   %s", action);

	DrawShadowedText(named, at, y, scale, kActionName);
}

void DrawResumeCountdown(float x, float y, float s, float width, int window)
{

	int remaining = DummyRecorder::GetLeadInRemaining();
	const char* label = "RECORDING IN %dF";

	if (remaining > 0)
	{
		window = DummyRecorder::GetLeadInLength();
	}
	else
	{
		remaining = FrameStepper::GetResumeCountdown();
		label = "RESUME IN %dF";
	}

	if (remaining <= 0 || window <= 0)
		return;

	char text[64] = {};
	sprintf_s(text, label, remaining);

	const float left = static_cast<float>(remaining) / static_cast<float>(window);
	const float scale = kTextScale * s * (1.0f + left * 0.8f);

	DrawShadowedText(text, x, y, scale, kAdvPlus);

	const float barY = y + GameFont::GetLineHeight() * scale + 2.0f * s;
	const float barHeight = 4.0f * s;

	QuadRenderer::FillRect(x, barY, width, barHeight, 0xC0202020);
	QuadRenderer::FillRect(x, barY, width * left, barHeight, Fade(kAdvPlus));
}

}

bool FrameMeterHud::IsVisible()
{
	return g_visible;
}

void FrameMeterHud::SetVisible(bool visible)
{
	g_visible = visible;
}

void FrameMeterHud::Toggle()
{
	g_visible = !g_visible;
}

bool FrameMeterHud::HasGameAssets()
{
	return GameFont::IsLoaded();
}

void FrameMeterHud::OnDeviceLost()
{
	QuadRenderer::OnDeviceLost();
}

void FrameMeterHud::Render(IDirect3DDevice9* device)
{
	const bool countdownVisible = FrameStepper::GetResumeCountdown() > 0 ||
		DummyRecorder::GetLeadInRemaining() > 0;

	if ((!g_visible && !countdownVisible) || device == nullptr)
		return;

	const FrameMeter::AutoPauseConfig autoPause = FrameMeter::GetAutoPause();

	if (OnlineState::IsOnline())
	{
		g_visible = false;
		FrameMeter::Reset();
		return;
	}

	if (!GameState::AllowsTrainingTools())
		return;

	if (!GameState::IsBattleTicking() && !FrameStepper::IsPaused())
		return;

	EnsureAssets(device);

	const int length = FrameMeter::GetLength();

	D3DVIEWPORT9 viewport = {};
	if (FAILED(device->GetViewport(&viewport)))
		return;

	const float s = g_modVals.frameMeterScale;
	const float lineHeight = GameFont::GetLineHeight() * kTextScale * s;
	const float width = kTrackWidth * s;
	const float totalsHeight = g_modVals.frameMeterTotals ? (lineHeight + kTextGap * s) * 2.0f : 0.0f;
	const float attrHeight = g_modVals.frameMeterAttributes
		? (kAttrRowHeight + kAttrRowGap) * s : 0.0f;
	const float height = lineHeight * 2.0f + kTextGap * s * 2.0f
		+ kRowHeight * s * 2.0f + kRowGap * s + attrHeight * 2.0f + totalsHeight;

	const float autoX = (viewport.Width - width) * 0.5f;
	const float autoY = viewport.Height * 0.87f - height;

	const bool placed = !g_modVals.frameMeterAuto && g_modVals.frameMeterX >= 0 &&
		g_modVals.frameMeterY >= 0;

	float x = placed ? static_cast<float>(g_modVals.frameMeterX) : autoX;
	float y = placed ? static_cast<float>(g_modVals.frameMeterY) : autoY;

	// Never placed and not asking for automatic: take the automatic spot once and keep it, so the
	// meter starts where it belongs and can still be dragged without visiting a checkbox first.
	if (!placed && !g_modVals.frameMeterAuto && viewport.Width > 0)
	{
		g_modVals.frameMeterX = static_cast<int>(x < 0.0f ? 0.0f : x);
		g_modVals.frameMeterY = static_cast<int>(y < 0.0f ? 0.0f : y);

		Settings::SaveInt("FrameMeter", "PositionX", g_modVals.frameMeterX);
		Settings::SaveInt("FrameMeter", "PositionY", g_modVals.frameMeterY);
	}

	if (x > viewport.Width - 60.0f)
		x = viewport.Width - 60.0f;
	if (y > viewport.Height - 40.0f)
		y = viewport.Height - 40.0f;
	if (x < 0.0f)
		x = 0.0f;
	if (y < 0.0f)
		y = 0.0f;

	if (g_visible && DragMeter(viewport, x, y, width, height))
	{
		x = static_cast<float>(g_modVals.frameMeterX);
		y = static_cast<float>(g_modVals.frameMeterY);
	}

	if (!QuadRenderer::Begin(device))
		return;

	float cursor = y;

	if (g_visible)
	{
		if (g_modVals.frameMeterTotals)
			cursor += DrawLineTotals(x, cursor, s, 0, length) ? lineHeight + kTextGap * s : 0.0f;

		DrawReadout(x, cursor, s, 0);
		cursor += lineHeight + kTextGap * s;

		DrawTrack(x, cursor, s, 0, length);
		if (g_modVals.frameMeterCounts)
			DrawRunCounts(x, cursor, s, 0, length);
		DrawTrailingCounter(x, cursor, s, 0, length);
		cursor += kRowHeight * s;

		if (g_modVals.frameMeterAttributes)
		{
			cursor += kAttrRowGap * s;
			DrawAttributeTrack(x, cursor, s, 0, length);
			cursor += kAttrRowHeight * s;
		}

		cursor += kRowGap * s;

		DrawTrack(x, cursor, s, 1, length);
		if (g_modVals.frameMeterCounts)
			DrawRunCounts(x, cursor, s, 1, length);
		DrawTrailingCounter(x, cursor, s, 1, length);
		cursor += kRowHeight * s;

		if (g_modVals.frameMeterAttributes)
		{
			cursor += kAttrRowGap * s;
			DrawAttributeTrack(x, cursor, s, 1, length);
			cursor += kAttrRowHeight * s;
		}

		cursor += kTextGap * s;

		DrawReadout(x, cursor, s, 1);
		cursor += lineHeight + kTextGap * s;

		if (g_modVals.frameMeterTotals)
			cursor += DrawLineTotals(x, cursor, s, 1, length) ? lineHeight + kTextGap * s : 0.0f;
	}

	if (countdownVisible)
		DrawResumeCountdown(x, cursor, s, width, autoPause.resumeDelayFrames);

	QuadRenderer::End();
}
