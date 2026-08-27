#include "Overlay/Window/MusicWindow.h"

#include "Overlay/UiScale.h"

#include <imgui.h>

MusicWindow::MusicWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void MusicWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(620.0f), Ui::Scaled(520.0f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Ui::Scaled(460.0f), Ui::Scaled(320.0f)),
		ImVec2(FLT_MAX, FLT_MAX));
}

void MusicWindow::Draw()
{
	m_panel.Draw();
}
