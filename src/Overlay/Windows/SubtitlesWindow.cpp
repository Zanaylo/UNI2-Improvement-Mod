#include "Overlay/Windows/SubtitlesWindow.h"

#include "Overlay/Widgets/UiScale.h"

#include <imgui.h>

#include <cfloat>

SubtitlesWindow::SubtitlesWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void SubtitlesWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(820.0f), Ui::Scaled(640.0f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Ui::Scaled(520.0f), Ui::Scaled(360.0f)),
		ImVec2(FLT_MAX, FLT_MAX));
}

void SubtitlesWindow::Draw()
{
	m_panel.Draw();
}
