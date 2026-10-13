#include "Overlay/Windows/GameDebugWindow.h"

#include "Game/Debug/GameDebug.h"
#include "Game/Debug/ScriptConsole.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

#include <cfloat>

GameDebugWindow::GameDebugWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void GameDebugWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(520.0f), Ui::Scaled(520.0f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Ui::Scaled(420.0f), Ui::Scaled(300.0f)),
		ImVec2(FLT_MAX, FLT_MAX));
}

void GameDebugWindow::Draw()
{
	ImGui::TextWrapped("Switches French Bread left in the battle scripts. Changes apply outside a match.");

	ImGui::Spacing();
	ImGui::SeparatorText("Messages and sound");
	DrawOptions(false);

	ImGui::Spacing();
	ImGui::SeparatorText("Changes the game, offline only");
	DrawOptions(true);

	ImGui::Spacing();
	DrawStatus();
}

void GameDebugWindow::DrawOptions(bool changesTheGame)
{
	for (int i = 0; i < GameDebug::OptionCount(); ++i)
	{
		const GameDebug::Option& option = GameDebug::OptionAt(i);

		if (option.changesTheGame != changesTheGame)
			continue;

		ImGui::PushID(option.key);

		if (ImGui::Checkbox(option.label, option.value))
			GameDebug::Save(i);

		UiText::Help(option.help);
		ImGui::PopID();
	}
}

void GameDebugWindow::DrawStatus()
{
	ImGui::SeparatorText("Status");
	UiText::Muted("Scripts: %s", GameDebug::StatusText());

	if (GameDebug::HoldsBackGameChanges())
		UiText::Warn("The options that change the game are held back: you are online, or this game version "
			"cannot reload its scripts.");

	if (ScriptConsole::IsOpen())
		UiText::Good("%ld script message(s) written to the log.", ScriptConsole::Lines());
}
