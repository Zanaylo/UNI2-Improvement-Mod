#include "Overlay/Window/RestartPrompt.h"

#include "Game/GameRestart.h"
#include "Overlay/UiText.h"

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
