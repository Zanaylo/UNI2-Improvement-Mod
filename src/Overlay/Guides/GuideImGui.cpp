#include "Overlay/Guides/GuideImGui.h"

#include "Overlay/Widgets/UiScale.h"

#include <imgui.h>

namespace {

ImU32 Rgba(uint32_t argb)
{
	return IM_COL32((argb >> 16) & 0xff, (argb >> 8) & 0xff, argb & 0xff, (argb >> 24) & 0xff);
}

bool HasSwatch(const GuideRow& row)
{
	return row.colour != kGuideNoSwatch;
}

void Swatch(uint32_t colour)
{
	const float box = ImGui::GetTextLineHeight();
	const ImVec2 at = ImGui::GetCursorScreenPos();
	ImDrawList* const draw = ImGui::GetWindowDrawList();

	draw->AddRectFilled(at, ImVec2(at.x + box, at.y + box), Rgba(colour));
	draw->AddRect(at, ImVec2(at.x + box, at.y + box), IM_COL32(0, 0, 0, 160));

	ImGui::Dummy(ImVec2(box, box));
	ImGui::SameLine(0.0f, Ui::Scaled(6.0f));
}

void Grid(const GuidePage& page)
{
	const float box = ImGui::GetTextLineHeight();
	const float columnGap = Ui::Scaled(18.0f);

	float widest = 0.0f;
	for (int i = 0; i < page.rowCount; ++i)
	{
		const float w = HasSwatch(page.rows[i]) ? ImGui::CalcTextSize(page.rows[i].name).x : 0.0f;
		if (w > widest)
			widest = w;
	}

	if (widest <= 0.0f)
		return;

	const float column = box + Ui::Scaled(6.0f) + widest + columnGap;
	const float available = ImGui::GetContentRegionAvail().x;
	const int perRow = available < column * 2.0f ? 1 : static_cast<int>(available / column);
	const float startX = ImGui::GetCursorPosX();

	int placed = 0;
	for (int i = 0; i < page.rowCount; ++i)
	{
		const GuideRow& row = page.rows[i];
		if (!HasSwatch(row))
			continue;

		if (placed % perRow != 0)
			ImGui::SameLine(startX + column * static_cast<float>(placed % perRow));

		++placed;

		Swatch(row.colour);
		ImGui::TextUnformatted(row.name);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", row.detail);
	}
}

void List(const GuidePage& page)
{
	for (int i = 0; i < page.rowCount; ++i)
	{
		const GuideRow& row = page.rows[i];
		if (!HasSwatch(row))
			continue;

		ImGui::PushID(i);
		Swatch(row.colour);
		ImGui::SeparatorText(row.name);
		ImGui::TextWrapped("%s", row.summary);
		ImGui::Spacing();
		ImGui::TextWrapped("%s", row.detail);
		ImGui::Spacing();
		ImGui::PopID();
	}
}

void Notes(const GuidePage& page)
{
	for (int i = 0; i < page.rowCount; ++i)
	{
		const GuideRow& row = page.rows[i];

		if (!HasSwatch(row))
			ImGui::BulletText("%s: %s", row.name, row.detail);
	}
}

}

void GuideImGui::Draw(const GuideContent& content, Layout layout)
{
	for (int i = 0; i < content.pageCount; ++i)
	{
		const GuidePage& page = content.pages[i];

		ImGui::Spacing();
		ImGui::SeparatorText(page.title);
		ImGui::TextDisabled("%s", page.heading);

		Notes(page);

		if (layout == Layout_Grid)
			Grid(page);
		else
			List(page);
	}
}
