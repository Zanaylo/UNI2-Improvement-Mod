#include "Overlay/Panels/RestartPrompt.h"

#include "Game/Battle/GameRestart.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

void RestartPrompt::Draw(const char* reason)
{
	ImGui::Separator();
	UiText::Warn("%s", reason);

	if (!GameRestart::CanSoftReset())
	{
		UiText::Muted("%s", GameRestart::StatusText());
		return;
	}

	ImGui::BeginDisabled(GameRestart::IsPending());

	if (ImGui::Button("Restart the game"))
		GameRestart::SoftReset();

	ImGui::EndDisabled();
}
