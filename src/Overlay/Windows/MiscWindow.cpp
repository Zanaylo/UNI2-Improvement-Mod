#include "Overlay/Windows/MiscWindow.h"

#include "Overlay/Widgets/UiScale.h"

#include <imgui.h>

#include <cfloat>

MiscWindow::MiscWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void MiscWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(560.0f), Ui::Scaled(440.0f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Ui::Scaled(460.0f), Ui::Scaled(320.0f)),
		ImVec2(FLT_MAX, FLT_MAX));
}

void MiscWindow::Draw()
{
	m_panel.Draw();
}
