#include "Overlay/Window/HitboxLegendWindow.h"

#include "Overlay/Window/HitboxOverlay.h"

namespace {

ImU32 Rgba(unsigned int color)
{
	return color;
}

void Swatch(unsigned int color)
{
	const ImVec2 at = ImGui::GetCursorScreenPos();
	const float size = ImGui::GetTextLineHeight();

	ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + size, at.y + size), Rgba(color));
	ImGui::GetWindowDrawList()->AddRect(at, ImVec2(at.x + size, at.y + size),
		IM_COL32(0, 0, 0, 160));

	ImGui::Dummy(ImVec2(size, size));
	ImGui::SameLine(0.0f, 6.0f);
}

}

HitboxLegendWindow::HitboxLegendWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void HitboxLegendWindow::BeforeDraw()
{

	const ImGuiViewport* const viewport = ImGui::GetMainViewport();
	const float maxWidth = viewport->WorkSize.x;
	const float maxHeight = viewport->WorkSize.y;

	const float width = maxWidth * 0.52f < 520.0f ? maxWidth : maxWidth * 0.52f;
	const float height = maxHeight * 0.70f < 320.0f ? maxHeight : maxHeight * 0.70f;

	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 240.0f), ImVec2(maxWidth, maxHeight));
}

void HitboxLegendWindow::Draw()
{
	ImGui::SeparatorText("What touches what");

	ImGui::BulletText("A hit is a Hitbox overlapping a Hurtbox.");
	ImGui::BulletText("A throw ignores hurtboxes. It only misses if you are airborne.");
	ImGui::BulletText("Pushboxes only push.");
	ImGui::BulletText("Markers and grab points are points. Nothing collides with them.");

	ImGui::Spacing();
	ImGui::SeparatorText("The box types");

	for (int i = 0; i < HitboxOverlay::BoxCategory_COUNT; ++i)
	{
		ImGui::PushID(i);

		Swatch(HitboxOverlay::GetCategoryColor(i));
		ImGui::SeparatorText(HitboxOverlay::GetCategoryName(i));

		ImGui::TextWrapped("%s", HitboxOverlay::GetCategorySummary(i));
		ImGui::Spacing();
		ImGui::TextWrapped("%s", HitboxOverlay::GetCategoryDetail(i));
		ImGui::Spacing();

		ImGui::PopID();
	}

}
