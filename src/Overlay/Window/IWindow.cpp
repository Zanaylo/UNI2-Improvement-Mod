#include "Overlay/Window/IWindow.h"

IWindow::IWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: m_title(title)
	, m_isOpen(false)
	, m_closable(closable)
	, m_windowFlags(windowFlags)
{
}

void IWindow::Update()
{
	if (!m_isOpen)
		return;

	BeforeDraw();

	if (ImGui::Begin(m_title.c_str(), m_closable ? &m_isOpen : nullptr, m_windowFlags))
	{
		Draw();

		if (GrowsToFitContent())
		{
			const float needed = ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y;
			const ImVec2 size = ImGui::GetWindowSize();

			if (needed > size.y)
				ImGui::SetWindowSize(ImVec2(size.x, needed));
		}
	}

	ImGui::End();

	AfterDraw();
}
