#include "Overlay/Guides/FrameMeterLegend.h"
#include "Overlay/Guides/GuideImGui.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Windows/FrameMeterLegendWindow.h"

#include "Training/Meter/FrameMeter.h"

#include <cfloat>

namespace {

struct Sample
{
	FrameMeter::State state;
	int frames;
};

const Sample kSample[] = {
	{ FrameMeter::State::Startup,  6 },
	{ FrameMeter::State::Active,   3 },
	{ FrameMeter::State::Recovery, 9 },
	{ FrameMeter::State::Idle,     6 },
};

constexpr int kSampleInvulnFrom = 1;
constexpr int kSampleInvulnTo = 7;

constexpr int kSampleActiveFrom = 6;
constexpr int kSampleActiveTo = 8;

ImU32 Rgba(unsigned int argb)
{
	return IM_COL32((argb >> 16) & 0xff, (argb >> 8) & 0xff, argb & 0xff, (argb >> 24) & 0xff);
}

void Cell(ImDrawList* draw, ImVec2 at, float width, float height, unsigned int color)
{
	const float r = static_cast<float>((color >> 16) & 0xff);
	const float g = static_cast<float>((color >> 8) & 0xff);
	const float b = static_cast<float>(color & 0xff);

	const ImU32 top = IM_COL32(static_cast<int>(r * 1.25f > 255.0f ? 255.0f : r * 1.25f),
		static_cast<int>(g * 1.25f > 255.0f ? 255.0f : g * 1.25f),
		static_cast<int>(b * 1.25f > 255.0f ? 255.0f : b * 1.25f), 255);
	const ImU32 bottom = IM_COL32(static_cast<int>(r * 0.72f), static_cast<int>(g * 0.72f),
		static_cast<int>(b * 0.72f), 255);

	draw->AddRectFilledMultiColor(at, ImVec2(at.x + width, at.y + height), top, top, bottom, bottom);
}

void DrawThinRow(ImDrawList* draw, ImVec2 origin, float cellW, float gap, float height, int from,
	int to, unsigned int color)
{
	for (int cell = from; cell <= to; ++cell)
	{
		const ImVec2 at(origin.x + cell * cellW, origin.y);
		draw->AddRectFilled(at, ImVec2(at.x + cellW - gap, at.y + height), Rgba(color));
	}
}

void DrawSampleMeter()
{
	const float scale = Ui::Scale();
	const float cellW = 11.0f * scale;
	const float gap = 1.0f * scale;
	const float rowH = 20.0f * scale;
	const float thinH = 9.0f * scale;
	const float thinGap = 3.0f * scale;

	int total = 0;
	for (int i = 0; i < IM_ARRAYSIZE(kSample); ++i)
		total += kSample[i].frames;

	const float width = total * cellW;

	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 bar(origin.x, origin.y + thinH + thinGap);

	DrawThinRow(draw, origin, cellW, gap, thinH, kSampleActiveFrom, kSampleActiveTo,
		FrameMeter::GetAttackMarkColor(FrameMeter::AttackMark_Head));

	int cell = 0;
	for (int i = 0; i < IM_ARRAYSIZE(kSample); ++i)
	{
		for (int f = 0; f < kSample[i].frames; ++f, ++cell)
			Cell(draw, ImVec2(bar.x + cell * cellW, bar.y), cellW - gap, rowH,
				FrameMeter::GetStateColor(kSample[i].state));
	}

	draw->AddRect(ImVec2(bar.x - 1.0f, bar.y - 1.0f), ImVec2(bar.x + width, bar.y + rowH + 1.0f),
		IM_COL32(255, 255, 255, 48));

	DrawThinRow(draw, ImVec2(bar.x, bar.y + rowH + thinGap), cellW, gap, thinH, kSampleInvulnFrom,
		kSampleInvulnTo, FrameMeter::GetMarkerColor(FrameMeter::Marker_StrikeInvuln));

	const float labelX = origin.x + width + Ui::Scaled(10.0f);
	const float lineH = ImGui::GetTextLineHeight();
	const ImU32 labelColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);

	draw->AddText(ImVec2(labelX, origin.y + (thinH - lineH) * 0.5f), labelColor, "Attack Row");
	draw->AddText(ImVec2(labelX, bar.y + (rowH - lineH) * 0.5f), labelColor, "Bar");
	draw->AddText(ImVec2(labelX, bar.y + rowH + thinGap + (thinH - lineH) * 0.5f), labelColor,
		"Invincibility Row");

	ImGui::Dummy(ImVec2(width, thinH + thinGap + rowH + thinGap + thinH));
}

}

FrameMeterLegendWindow::FrameMeterLegendWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void FrameMeterLegendWindow::BeforeDraw()
{

	const ImGuiViewport* const viewport = ImGui::GetMainViewport();
	const float maxWidth = viewport->WorkSize.x;
	const float maxHeight = viewport->WorkSize.y;

	const float width = maxWidth * 0.62f < 560.0f ? maxWidth : maxWidth * 0.62f;
	const float height = maxHeight * 0.55f < 260.0f ? maxHeight : maxHeight * 0.55f;

	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(Ui::Scaled(420.0f, 200.0f), ImVec2(maxWidth, maxHeight));
}

void FrameMeterLegendWindow::Draw()
{
	ImGui::TextUnformatted("Startup 7F  /  Total 18F  /  Advantage +4F");
	DrawSampleMeter();

	GuideImGui::Draw(FrameMeterLegend::Get(), GuideImGui::Layout_Grid);
}
