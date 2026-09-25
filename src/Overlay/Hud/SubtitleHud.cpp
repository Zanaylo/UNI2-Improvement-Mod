#include "Overlay/Hud/SubtitleHud.h"

#include "Core/Config/interfaces.h"
#include "Game/Tables/CharaTables.h"
#include "Game/Subtitles/SubtitleWatch.h"

#include <imgui.h>

#include <cstdio>

namespace {

constexpr float kLineGap = 1.15f;
constexpr float kOutline = 2.0f;
constexpr float kMinScale = 0.5f;
constexpr float kMaxScale = 4.0f;

float Scale()
{
	const float wanted = g_modVals.subtitleScale / 100.0f;

	if (wanted < kMinScale)
		return kMinScale;

	return wanted > kMaxScale ? kMaxScale : wanted;
}

void Label(const SubtitleWatch::Shown& line, char* out, int size)
{
	const char* const name = g_modVals.subtitleNames ? CharaTables::Name(line.chara) : nullptr;

	if (name == nullptr)
	{
		_snprintf_s(out, size, _TRUNCATE, "%s", line.text);
		return;
	}

	_snprintf_s(out, size, _TRUNCATE, "%s: %s", name, line.text);
}

void DrawLine(ImDrawList* list, const char* text, float centreX, float y, float size, float alpha)
{
	const ImVec2 measured = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
	const ImVec2 at(centreX - measured.x * 0.5f, y);

	const ImU32 shadow = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, alpha));
	const ImU32 body = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha));

	for (int dx = -1; dx <= 1; ++dx)
	{
		for (int dy = -1; dy <= 1; ++dy)
		{
			if (dx == 0 && dy == 0)
				continue;

			list->AddText(ImGui::GetFont(), size,
				ImVec2(at.x + dx * kOutline, at.y + dy * kOutline), shadow, text);
		}
	}

	list->AddText(ImGui::GetFont(), size, at, body, text);
}

}

bool SubtitleHud::IsShowing()
{
	return SubtitleWatch::HasAny();
}

void SubtitleHud::Draw()
{
	SubtitleWatch::Shown lines[SubtitleWatch::kMaxShown] = {};

	const int count = SubtitleWatch::Take(lines, SubtitleWatch::kMaxShown);

	if (count == 0)
		return;

	ImDrawList* const list = ImGui::GetBackgroundDrawList();
	const ImVec2 screen = ImGui::GetIO().DisplaySize;

	const float size = ImGui::GetFontSize() * Scale();
	const float step = size * kLineGap;
	const float top = screen.y * (g_modVals.subtitleY / 100.0f) - step * (count - 1);

	for (int i = 0; i < count; ++i)
	{
		char text[320] = {};
		Label(lines[i], text, sizeof(text));

		DrawLine(list, text, screen.x * 0.5f, top + step * i, size, lines[i].fade);
	}
}
