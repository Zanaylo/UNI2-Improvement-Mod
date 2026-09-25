#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Windows/HitboxLegendWindow.h"

#include "Overlay/Guides/GuideImGui.h"
#include "Overlay/Guides/HitboxLegend.h"

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
	ImGui::SetNextWindowSizeConstraints(Ui::Scaled(420.0f, 240.0f), ImVec2(maxWidth, maxHeight));
}

void HitboxLegendWindow::Draw()
{
	GuideImGui::Draw(HitboxLegend::Get(), GuideImGui::Layout_List);
}
