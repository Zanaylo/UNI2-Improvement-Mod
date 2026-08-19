#include "Overlay/Window/UpdateNotifierWindow.h"

#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/Settings.h"
#include "Web/UpdateCheck.h"

#include <Windows.h>

#include <imgui.h>

UpdateNotifierWindow::UpdateNotifierWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void UpdateNotifierWindow::BeforeDraw()
{
	ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 0.0f), ImVec2(520.0f, FLT_MAX));
}

void UpdateNotifierWindow::Draw()
{
	ImGui::Text("%s %s is out.", UNI2_IM_NAME, UpdateCheck::GetLatestVersion());
	ImGui::TextDisabled("You are running %s.", UNI2_IM_VERSION);

	ImGui::Spacing();

	if (ImGui::Button("Open the releases page"))
	{
		ShellExecuteA(nullptr, "open", UpdateCheck::GetReleaseUrl(), nullptr, nullptr, SW_SHOWNORMAL);
		UpdateCheck::Dismiss();
		Close();
	}

	ImGui::SameLine();

	if (ImGui::Button("Later"))
	{
		UpdateCheck::Dismiss();
		Close();
	}

	ImGui::SameLine();

	if (ImGui::Button("Stop checking"))
	{
		g_modVals.checkForUpdates = false;
		Settings::SaveInt("Mod", "CheckForUpdates", 0);
		UpdateCheck::Dismiss();
		Close();
	}

	ImGui::TextDisabled("The game locks the mod while it runs, so close it before replacing the dll.");
}
