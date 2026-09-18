#include "Overlay/UiScale.h"
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

constexpr int kSampleActiveFrom = 6;
constexpr int kSampleActiveTo = 8;

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
	FrameMeter::State::Backdash,
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

struct Swatch
{
	unsigned int color;
	const char* name;
};

void SwatchGrid(const Swatch* swatches, int count)
{
	const float box = ImGui::GetTextLineHeight();
	const float boxGap = Ui::Scaled(6.0f);
	const float columnGap = Ui::Scaled(18.0f);

	float widest = 0.0f;
	for (int i = 0; i < count; ++i)
	{
		const float w = ImGui::CalcTextSize(swatches[i].name).x;
		if (w > widest)
			widest = w;
	}

	const float column = box + boxGap + widest + columnGap;
	const float available = ImGui::GetContentRegionAvail().x;
	const int perRow = available < column * 2.0f ? 1 : static_cast<int>(available / column);
	const float startX = ImGui::GetCursorPosX();

	for (int i = 0; i < count; ++i)
	{
		if (i % perRow != 0)
			ImGui::SameLine(startX + column * static_cast<float>(i % perRow));

		const ImVec2 at = ImGui::GetCursorScreenPos();
		ImDrawList* const draw = ImGui::GetWindowDrawList();
		draw->AddRectFilled(at, ImVec2(at.x + box, at.y + box), Rgba(swatches[i].color));
		draw->AddRect(at, ImVec2(at.x + box, at.y + box), IM_COL32(0, 0, 0, 160));

		ImGui::Dummy(ImVec2(box, box));
		ImGui::SameLine(0.0f, boxGap);
		ImGui::TextUnformatted(swatches[i].name);
	}
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

	ImGui::Spacing();
	ImGui::SeparatorText("Bar");
	ImGui::TextWrapped("One cell per frame. What the character is doing.");

	Swatch bar[IM_ARRAYSIZE(kStates) + FrameMeter::kFirstInvulnMarker] = {};
	int count = 0;

	for (int i = 0; i < IM_ARRAYSIZE(kStates); ++i)
		bar[count++] = { FrameMeter::GetStateColor(kStates[i]), FrameMeter::GetStateName(kStates[i]) };

	for (int i = 0; i < FrameMeter::kFirstInvulnMarker; ++i)
	{
		const FrameMeter::Marker marker = static_cast<FrameMeter::Marker>(i);
		bar[count++] = { FrameMeter::GetMarkerColor(marker), FrameMeter::GetMarkerName(marker) };
	}

	SwatchGrid(bar, count);

	ImGui::Spacing();
	ImGui::SeparatorText("Attack Row (above the bar)");
	ImGui::TextWrapped("Only on active frames. The attack's properties.");

	Swatch attack[FrameMeter::AttackMark_COUNT] = {};
	for (int i = 0; i < FrameMeter::AttackMark_COUNT; ++i)
	{
		const FrameMeter::AttackMark mark = static_cast<FrameMeter::AttackMark>(i);
		attack[i] = { FrameMeter::GetAttackMarkColor(mark), FrameMeter::GetAttackMarkName(mark) };
	}

	SwatchGrid(attack, FrameMeter::AttackMark_COUNT);

	ImGui::BulletText("Head: whiffs on head invincibility.");
	ImGui::BulletText("Foot: whiffs on foot invincibility.");
	ImGui::BulletText("Air: whiffs on air invincibility.");
	ImGui::TextWrapped("Two or more split the cell.");

	ImGui::Spacing();
	ImGui::SeparatorText("Invincibility Row (under the bar)");
	ImGui::TextWrapped("What can't hit the character on that frame. Same colours as the attack row, so "
		"a Head attack whiffs on a Head cell.");

	Swatch invuln[FrameMeter::Marker_COUNT - FrameMeter::kFirstInvulnMarker] = {};
	count = 0;

	for (int i = FrameMeter::kFirstInvulnMarker; i < FrameMeter::Marker_COUNT; ++i)
	{
		const FrameMeter::Marker marker = static_cast<FrameMeter::Marker>(i);
		invuln[count++] = { FrameMeter::GetMarkerColor(marker), FrameMeter::GetMarkerName(marker) };
	}

	SwatchGrid(invuln, count);

	ImGui::BulletText("Everything: nothing can hit. Drawn alone.");
	ImGui::BulletText("Two or three at once split the cell.");

	ImGui::Spacing();
	ImGui::SeparatorText("Numbers");

	ImGui::BulletText("Startup: frames until the move can hit, first active frame included.");
	ImGui::BulletText("Total: start of the move to its end.");
	ImGui::BulletText("Advantage: who acts first. Positive means you.");
	ImGui::BulletText("Advantage in brackets: before the opponent teched. Can be off.");
	ImGui::BulletText("Blockstun: stun from the last hit blocked.");
	ImGui::BulletText("Hitstun: stun from the last hit taken.");
	ImGui::BulletText("Gap: free frames before the last hit. None means it was airtight.");
	ImGui::BulletText("Flash: super flash length in the move shown. Not on the bar.");
	ImGui::BulletText("Damage: hit damage of the last combo. Starts over with the next combo.");
	ImGui::BulletText("Burn / Poison: everything Wagner's burn or Uzuki's poison did, in a combo, on block or in neutral. Starts over when a new one is applied.");
	ImGui::BulletText("Chip: damage from blocked hits. Starts over when either side gets hit.");
	ImGui::BulletText("Self: HP Carmine spent on his own moves.");
	ImGui::BulletText("Heal: HP Carmine got back. Training mode's refill is not counted.");
	ImGui::TextWrapped("Self and Heal start over after 3 seconds without a change.");
}
