#include "Overlay/Window/UpdateNotifierWindow.h"

#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/Settings.h"
#include "Overlay/UiText.h"
#include "Web/UpdateCheck.h"
#include "Web/UpdateInstall.h"

#include <Windows.h>

#include <cstdio>

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

void UpdateNotifierWindow::DrawProgress(const UpdateInstall::Snapshot& snapshot)
{
	const Web::Job::Status& job = snapshot.job;

	if (job.state == Web::Job::State_Idle)
		return;

	if (job.state == Web::Job::State_Running && job.percent >= 0)
	{
		char overlay[64] = {};
		sprintf_s(overlay, "%d%%", job.percent);

		ImGui::ProgressBar(job.percent / 100.0f, ImVec2(-1.0f, 0.0f), overlay);
	}
	else if (job.state == Web::Job::State_Running)
	{
		ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()), ImVec2(-1.0f, 0.0f), "");
	}

	if (job.source[0] != '\0')
		UiText::Muted("%s - from %s", job.step, job.source);
	else if (job.step[0] != '\0')
		UiText::Muted("%s", job.step);

	if (job.state == Web::Job::State_Failed && job.error[0] != '\0')
		UiText::Warn("%s", job.error);
}

void UpdateNotifierWindow::DrawInstall()
{
	UpdateInstall::Snapshot snapshot = {};
	UpdateInstall::Read(snapshot);

	DrawProgress(snapshot);

	if (snapshot.busy)
	{
		if (ImGui::Button("Stop"))
			UpdateInstall::Cancel();

		return;
	}

	if (snapshot.staged)
	{
		UiText::Good("The update is ready. The game closes, the updater swaps the files and Steam "
			"starts it again.");

		return;
	}

	if (ImGui::Button("Update now"))
		UpdateInstall::Start();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Downloads the release, checks it, then closes the game so the updater "
			"can replace the dll. Steam starts the game again when it is done.");
	}
}

void UpdateNotifierWindow::Draw()
{
	ImGui::Text("%s %s is out.", UNI2_IM_NAME, UpdateCheck::GetLatestVersion());
	ImGui::TextDisabled("You are running %s.", UNI2_IM_VERSION);

	ImGui::Spacing();

	DrawInstall();

	if (ImGui::Button("Open the releases page"))
	{
		ShellExecuteA(nullptr, "open", UpdateCheck::GetReleaseUrl(), nullptr, nullptr,
			SW_SHOWNORMAL);
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

	ImGui::TextDisabled("The game locks the mod while it runs, so an update that is installed by "
		"hand needs the game closed first.");
}
