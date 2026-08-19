#include "Overlay/Window/FrameMeterLegendWindow.h"

#include "Training/FrameMeter.h"

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

const FrameMeter::State kStates[] = {
	FrameMeter::State::Startup,
	FrameMeter::State::Active,
	FrameMeter::State::Recovery,
	FrameMeter::State::Cancellable,
	FrameMeter::State::Blockstun,
	FrameMeter::State::Hitstun,
	FrameMeter::State::Parry,
	FrameMeter::State::Dodge,
	FrameMeter::State::Movement,
	FrameMeter::State::Jump,
	FrameMeter::State::AirMovement,
	FrameMeter::State::Idle,
};

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

void DrawSampleMeter()
{
	const float scale = ImGui::GetFontSize() / 13.0f;
	const float cellW = 11.0f * scale;
	const float gap = 1.0f * scale;
	const float rowH = 20.0f * scale;
	const float attrH = 9.0f * scale;
	const float attrGap = 3.0f * scale;

	int total = 0;
	for (int i = 0; i < IM_ARRAYSIZE(kSample); ++i)
		total += kSample[i].frames;

	const float width = total * cellW;

	ImDrawList* const draw = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();

	int cell = 0;
	for (int i = 0; i < IM_ARRAYSIZE(kSample); ++i)
	{
		for (int f = 0; f < kSample[i].frames; ++f, ++cell)
		{
			const ImVec2 at(origin.x + cell * cellW, origin.y);
			Cell(draw, at, cellW - gap, rowH, FrameMeter::GetStateColor(kSample[i].state));

			if (cell < kSampleInvulnFrom || cell > kSampleInvulnTo)
				continue;

			const ImVec2 attrAt(at.x, origin.y + rowH + attrGap);
			draw->AddRectFilled(attrAt, ImVec2(attrAt.x + cellW - gap, attrAt.y + attrH),
				Rgba(FrameMeter::GetMarkerColor(FrameMeter::Marker_StrikeInvuln)));
		}
	}

	draw->AddRect(ImVec2(origin.x - 1.0f, origin.y - 1.0f),
		ImVec2(origin.x + width, origin.y + rowH + 1.0f), IM_COL32(255, 255, 255, 48));

	ImGui::Dummy(ImVec2(width, rowH + attrGap + attrH));
}

void Entry(unsigned int color, const char* name, float columnWidth)
{
	const ImVec2 at = ImGui::GetCursorScreenPos();
	const float size = ImGui::GetTextLineHeight();

	ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + size, at.y + size), Rgba(color));
	ImGui::GetWindowDrawList()->AddRect(at, ImVec2(at.x + size, at.y + size),
		IM_COL32(0, 0, 0, 160));

	ImGui::Dummy(ImVec2(size, size));
	ImGui::SameLine(0.0f, 6.0f);
	ImGui::TextUnformatted(name);
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::Dummy(ImVec2(columnWidth - (ImGui::GetItemRectMax().x - at.x), 0.0f));
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
	ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 200.0f), ImVec2(maxWidth, maxHeight));
}

void FrameMeterLegendWindow::Draw()
{
	ImGui::TextUnformatted("Startup 7F  /  Total 18F  /  Advantage +4F");
	DrawSampleMeter();

	ImGui::Spacing();
	ImGui::SeparatorText("The bar");

	const float column = 190.0f;
	const int perRow = 3;

	int placed = 0;
	for (int i = 0; i < IM_ARRAYSIZE(kStates); ++i, ++placed)
	{
		if (placed % perRow != 0)
			ImGui::SameLine(0.0f, 8.0f);

		Entry(FrameMeter::GetStateColor(kStates[i]), FrameMeter::GetStateName(kStates[i]), column);
	}

	for (int i = 0; i < FrameMeter::kFirstInvulnMarker; ++i, ++placed)
	{
		const FrameMeter::Marker marker = static_cast<FrameMeter::Marker>(i);

		if (placed % perRow != 0)
			ImGui::SameLine(0.0f, 8.0f);

		Entry(FrameMeter::GetMarkerColor(marker), FrameMeter::GetMarkerName(marker), column);
	}

	ImGui::Spacing();
	ImGui::SeparatorText("Status Row");

	ImGui::TextWrapped("Everything in force on that frame at once, the cell split evenly between "
		"them. White means nothing can connect and is drawn on its own.");

	placed = 0;
	for (int i = FrameMeter::kFirstInvulnMarker; i < FrameMeter::Marker_COUNT; ++i, ++placed)
	{
		const FrameMeter::Marker marker = static_cast<FrameMeter::Marker>(i);

		if (placed % perRow != 0)
			ImGui::SameLine(0.0f, 8.0f);

		Entry(FrameMeter::GetMarkerColor(marker), FrameMeter::GetMarkerName(marker), column);
	}

	ImGui::Spacing();
	ImGui::SeparatorText("The Number");

	ImGui::BulletText("Startup - until the move can connect, first active frame included.");
	ImGui::BulletText("Total - from the move starting to it ending.");
	ImGui::BulletText("Advantage - who acts first, positive meaning you. The number in brackets is "
		"the advantage you had before the opponent teched (can be inaccurate sometimes).");
	ImGui::BulletText("Blockstun - how long the opponent was held after blocking.");
	ImGui::BulletText("Hitstun - how long the opponent was held after being hit.");
	ImGui::BulletText("Gap - free frames between two held runs.");
}
