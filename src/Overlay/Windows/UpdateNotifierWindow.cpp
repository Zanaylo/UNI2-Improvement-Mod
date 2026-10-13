#include "Overlay/Windows/UpdateNotifierWindow.h"

#include "Core/ShellOpen.h"
#include "Core/info.h"
#include "Core/Config/interfaces.h"
#include "Core/Config/Settings.h"
#include "Overlay/Widgets/UiText.h"
#include "Web/UpdateCheck.h"
#include "Web/UpdateInstall.h"

#include <Windows.h>

#include <imgui.h>

UpdateNotifierWindow::UpdateNotifierWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void UpdateNotifierWindow::BeforeDraw()
{
	ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f, 0.0f), ImVec2(560.0f, FLT_MAX));
}

void UpdateNotifierWindow::DrawInstall()
{
	if (ImGui::Button("Update now"))
		UpdateInstall::Start();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Closes the game and installs the update. Steam starts the game again "
			"when it is done.");
	}

	const char* const error = UpdateInstall::GetError();

	if (error[0] != '\0')
		UiText::Warn("%s", error);
}

void UpdateNotifierWindow::Draw()
{
	ImGui::Text("%s %s is out.", UNI2_IM_NAME, UpdateCheck::GetLatestVersion());
	ImGui::TextDisabled("You are running %s.", UNI2_IM_VERSION);

	ImGui::Spacing();

	DrawInstall();

	if (ImGui::Button("Open the releases page"))
	{
		ShellOpen::Open(UpdateCheck::GetReleaseUrl());
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

	ImGui::TextDisabled("To install an update by hand, close the game first. The mod files are "
		"locked while it runs.");
}
