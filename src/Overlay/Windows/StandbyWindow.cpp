#include "Overlay/Windows/StandbyWindow.h"

#include "Core/Boot/Standby.h"
#include "Core/info.h"
#include "Overlay/Framework/WindowManager.h"
#include "Overlay/Widgets/Cards.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"
#include "Web/UpdateCheck.h"

#include <imgui.h>

#include <cstdio>

namespace {

constexpr float kWidth = 460.0f;

}

StandbyWindow::StandbyWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
	Open();
}

void StandbyWindow::BeforeDraw()
{
	const ImGuiIO& io = ImGui::GetIO();

	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver,
		ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(Ui::Scaled(kWidth, 0.0f), ImGuiCond_FirstUseEver);
}

void StandbyWindow::Draw()
{
	DrawReason();
	ImGui::Spacing();
	DrawUpdateState();
}

void StandbyWindow::DrawReason() const
{
	char text[256] = {};

	if (Standby::GameAge() == GameBuild::Age::Older)
	{
		sprintf_s(text, "This copy of UNI2 is older than %s. Update the game in Steam: the mod stays off until "
			"then.", UNI2_IM_SUPPORTED_GAME_VERSION);
	}
	else
	{
		sprintf_s(text, "UNI2 was updated. The mod only knows %s, so it stays off until its own update comes "
			"out.", UNI2_IM_SUPPORTED_GAME_VERSION);
	}

	Cards::Problem(text);
}

void StandbyWindow::DrawUpdateState() const
{
	if (UpdateCheck::IsChecking())
	{
		UiText::Muted("Looking for a mod update...");
		return;
	}

	if (UpdateCheck::HasNewer())
	{
		UiText::Good("Version %s is out.", UpdateCheck::GetLatestVersion());

		if (ImGui::Button("See the update"))
			WindowManager::GetInstance().OpenUpdateNotifier();

		return;
	}

	UiText::Muted("%s", UpdateCheck::HasAnswer() ? "No update yet. The mod checks again every 15 minutes."
		: UpdateCheck::GetStatusText());

	if (ImGui::Button("Check now"))
		UpdateCheck::Refresh();
}
