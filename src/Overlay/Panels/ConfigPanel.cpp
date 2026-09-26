#include "Overlay/Panels/ConfigPanel.h"

#include "Overlay/Widgets/UiScale.h"

#include "Core/Config/Hotkeys.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/Config/keycodes.h"
#include "Core/Input/BackgroundKeyboard.h"
#include "Core/Input/KeyboardCapture.h"
#include "Core/Input/PadInput.h"
#include "Core/info.h"
#include "Core/utils.h"
#include "Game/Battle/KeyboardSeat.h"
#include "Game/Battle/ScreenShake.h"
#include "Game/Engine/OnlineState.h"
#include "Overlay/Framework/WindowManager.h"
#include "Overlay/Panels/GraphicsPanel.h"
#include "Overlay/Widgets/UiText.h"
#include "Web/UpdateCheck.h"
#include "Web/UpdateInstall.h"

#include <Windows.h>
#include <imgui.h>
#include <string>

namespace {

constexpr int kFunctionRow = Hotkeys::Action_Count;

int g_bindCapture = -1;
bool g_bindPad = false;

void SetBindCapture(int index, bool pad)
{
	g_bindCapture = index;
	g_bindPad = pad;
	KeyboardCapture::SetKeyCaptureActive(index >= 0 && !pad);
}

void DrawKeyboardTab();
void DrawConfigSection();
void DrawConfigGeneralTab();
void DrawKeybindsTab();
void CaptureBind();
void DrawFunctionBinds();
void DrawBindRow(Hotkeys::Action action);
void DrawBindConflicts();

void DrawBackgroundKeyboard()
{
	ImGui::Spacing();

	ImGui::BeginDisabled(!BackgroundKeyboard::IsAvailable());

	bool background = BackgroundKeyboard::IsEnabled();
	if (ImGui::Checkbox("Keep the keyboard working when the game is in the background", &background))
		BackgroundKeyboard::SetEnabled(background);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The game stops reading the keyboard when you click another window. Turn this "
			"on and your keys keep reaching it, even while you type somewhere else.\n"
			"Controllers always work in the background.");
	}

	ImGui::EndDisabled();
}

void DrawKeyboardTab()
{
	ImGui::TextWrapped("The game puts the keyboard and the first controller on the same side, so in "
		"local versus they control the same character. Pick a side for the keyboard here and the "
		"controller moves to the other one. Your key settings are not changed.");

	ImGui::Spacing();

	if (!KeyboardSeat::IsAvailable())
	{
		ImGui::TextDisabled("The game is not mapped yet.");
		return;
	}

	const int seats[] = { KeyboardSeat::Seat_Default, KeyboardSeat::Seat_P1, KeyboardSeat::Seat_P2 };
	const int current = KeyboardSeat::GetSeat();

	Ui::SetItemWidth(160.0f);

	if (ImGui::BeginCombo("Keyboard plays", KeyboardSeat::GetSeatName(current)))
	{
		for (int seat : seats)
		{
			if (ImGui::Selectable(KeyboardSeat::GetSeatName(seat), seat == current))
				KeyboardSeat::SetSeat(seat);
		}

		ImGui::EndCombo();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Game default leaves the keyboard where the game puts it.\n"
			"1P and 2P keep your keys as they are and move the controller to the other side.");
	}

	bool route = KeyboardSeat::GetRouteSides();
	if (ImGui::Checkbox("Hold the side during a match", &route))
		KeyboardSeat::SetRouteSides(route);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Keeps you on the side you picked for the whole match. When off, the game "
			"decides who goes where and only the controller moves.");
	}

	ImGui::Spacing();
	ImGui::TextWrapped("If a second keyboard player is set up in the game's options, those keys "
		"will work on the controller's side. Set Keyboard Player Number to 1 there to turn them off.");

	DrawBackgroundKeyboard();

	ImGui::Spacing();
	ImGui::TextDisabled("%s", KeyboardSeat::GetStatus());

	if (OnlineState::IsOnline())
		ImGui::TextDisabled("Online: the sides are left alone until the match ends.");
}

void DrawConfigSection()
{
	if (!ImGui::CollapsingHeader("Config"))
	{
		SetBindCapture(-1, false);
		return;
	}

	if (!ImGui::BeginTabBar("##config"))
		return;

	if (ImGui::BeginTabItem("General"))
	{
		DrawConfigGeneralTab();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Keybinds"))
	{
		DrawKeybindsTab();
		ImGui::EndTabItem();
	}
	else
	{
		SetBindCapture(-1, false);
	}

	if (ImGui::BeginTabItem("Keyboard"))
	{
		DrawKeyboardTab();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void DrawConfigGeneralTab()
{
	ImGui::Text("%s %s", UNI2_IM_NAME, UNI2_IM_VERSION);
	ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

	if (ImGui::Checkbox("Check for updates on start", &g_modVals.checkForUpdates))
		Settings::SaveInt("Mod", "CheckForUpdates", g_modVals.checkForUpdates ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Checks GitHub once for a newer release when the game starts. Nothing is "
			"downloaded unless you ask.");
	}

	ImGui::BeginDisabled(UpdateCheck::IsChecking() || UpdateInstall::IsBusy());

	if (ImGui::Button("Check now"))
		UpdateCheck::Refresh();

	ImGui::EndDisabled();

	if (UpdateCheck::HasNewer())
	{
		ImGui::SameLine();

		if (ImGui::Button("Show the update"))
			WindowManager::GetInstance().OpenUpdateNotifier();
	}

	UiText::Muted("%s", UpdateCheck::GetStatusText());

	if (ImGui::Checkbox("Show hitboxes and frame meter in the pause menu",
		&g_modVals.drawWhilePaused))
	{
		Settings::SaveInt("Overlay", "DrawWhileGamePaused", g_modVals.drawWhilePaused ? 1 : 0);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The hitboxes and frame meter normally hide when you open the game's pause "
			"menu. Turn this on to keep them on screen while the match is paused.");
	}

	bool& blockMouse = WindowManager::GetInstance().GetBlockGameMouse();
	if (ImGui::Checkbox("Block mouse input to the game", &blockMouse))
		Settings::SaveInt("Overlay", "BlockGameMouse", blockMouse ? 1 : 0);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The game ignores the mouse, so clicking the overlay never affects it.\n"
			"Saved as soon as it changes.");

	int shake = ScreenShake::GetIntensity();

	ImGui::BeginDisabled(!ScreenShake::IsAvailable());

	Ui::SetItemWidth(160.0f);

	if (ImGui::SliderInt("Screen shake strength", &shake, 0, ScreenShake::kFullPercent,
		"%d%%"))
		ScreenShake::SetIntensity(shake);

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		g_modVals.screenShake = ScreenShake::GetIntensity();
		Settings::SaveInt("Video", "ScreenShake", g_modVals.screenShake);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Scales every screen shake in the game (Wald's walk, heavy hits, "
			"cutscenes). Shakes keep their length and timing and only move less. 100%% is the "
			"game's default and 0 turns shaking off.");
	}

	ImGui::EndDisabled();

	if (!ScreenShake::IsAvailable())
		UiText::Warn("%s", ScreenShake::StatusText());

	if (ImGui::Checkbox("Advanced stage options", &g_modVals.advancedStages))
		Settings::SaveInt("Stages", "AdvancedOptions", g_modVals.advancedStages ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Shows Light and Size on the stage list, and the placement sliders under "
			"it. Light only affects glowing (additive) parts, which UNI2's own stages do not have. "
			"The right Size is different on every stage.");
	}

	GraphicsPanel::DrawOverlayAppearance();

	ImGui::SeparatorText("Holding the next-frame key");

	Ui::SetItemWidth(160.0f);
	ImGui::SliderInt("Wait before repeating", &g_modVals.stepRepeatDelayMs, 0, 1000, "%d ms");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("Training", "StepRepeatDelayMs", g_modVals.stepRepeatDelayMs);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("How long to hold the key before it starts stepping by itself. A tap always "
			"steps one frame.");
	}

	Ui::SetItemWidth(160.0f);
	ImGui::SliderInt("Between steps", &g_modVals.stepRepeatIntervalMs, 16, 500, "%d ms");
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveInt("Training", "StepRepeatIntervalMs", g_modVals.stepRepeatIntervalMs);

	if (ImGui::IsItemHovered())
	{

		ImGui::SetTooltip("Time between steps while the key is held (%d a second). It rounds to "
			"whole frames, so at 60 Hz anything from 34 to 49 ms steps every third frame.",
			1000 / (g_modVals.stepRepeatIntervalMs > 0 ? g_modVals.stepRepeatIntervalMs : 1));
	}

}

void DrawKeybindsTab()
{
	if (g_bindCapture >= 0)
		CaptureBind();

	DrawFunctionBinds();

	if (!ImGui::BeginTable("##keybinds", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("What");
	ImGui::TableSetupColumn("Key");
	ImGui::TableSetupColumn("Pad");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();

	for (int i = 0; i < Hotkeys::Action_Count; ++i)
		DrawBindRow(static_cast<Hotkeys::Action>(i));

	ImGui::EndTable();

	if (g_bindCapture >= 0)
		ImGui::TextDisabled("%s", g_bindPad ? "Press a pad button, or Escape to cancel."
			: "Press the key you want, or Escape to cancel.");
	else
		ImGui::TextDisabled("Saved to UNI2_IM.ini as soon as something is bound.");

	DrawBindConflicts();
}

void CaptureBind()
{
	const Hotkeys::Action action = static_cast<Hotkeys::Action>(g_bindCapture);

	if (!g_bindPad)
		KeyboardCapture::SetKeyCaptureActive(true);

	const int pressed = PollPressedKey();

	if (pressed == VK_ESCAPE)
	{
		SetBindCapture(-1, false);
		return;
	}

	if (g_bindPad)
	{
		const int button = PadInput::PollPressedButton();

		if (button == PadInput::kNone)
			return;

		if (g_bindCapture == kFunctionRow)
			Hotkeys::SetFunctionButton(button);
		else
			Hotkeys::SetPadButton(action, button);

		SetBindCapture(-1, false);
		return;
	}

	if (pressed == 0)
		return;

	if (g_bindCapture == kFunctionRow)
		Hotkeys::SetFunctionKey(pressed);
	else
		Hotkeys::SetKey(action, pressed, Hotkeys::GetKeyNeedsFunction(action));

	SetBindCapture(-1, false);
}

void DrawFunctionBinds()
{
	ImGui::TextUnformatted("Function");
	ImGui::SameLine();
	ImGui::TextDisabled("hold it with another key or button for shortcuts");

	const bool keyCapturing = g_bindCapture == kFunctionRow && !g_bindPad;
	const bool padCapturing = g_bindCapture == kFunctionRow && g_bindPad;

	const int key = Hotkeys::GetFunctionKey();

	ImGui::PushID("function");

	if (ImGui::SmallButton(keyCapturing ? "Cancel" : "Change key"))
		SetBindCapture(keyCapturing ? -1 : kFunctionRow, false);

	ImGui::SameLine();
	ImGui::TextUnformatted(keyCapturing ? "press a key"
		: (key != 0 ? GetNameFromVirtualKey(key) : "none"));

	ImGui::SameLine();

	if (ImGui::SmallButton(padCapturing ? "Cancel##pad" : "Change button"))
		SetBindCapture(padCapturing ? -1 : kFunctionRow, true);

	ImGui::SameLine();
	ImGui::TextUnformatted(padCapturing ? "press a button"
		: PadInput::GetButtonName(Hotkeys::GetFunctionButton()));

	ImGui::SameLine();
	ImGui::TextDisabled(PadInput::IsConnected() ? "(pad found)" : "(no pad)");

	ImGui::PopID();

	if (key == 0)
		ImGui::TextDisabled("Without a function key, a keyboard bind marked Fn cannot fire.");
}

void DrawBindRow(Hotkeys::Action action)
{
	const bool keyCapturing = g_bindCapture == action && !g_bindPad;
	const bool padCapturing = g_bindCapture == action && g_bindPad;

	const int key = Hotkeys::GetKey(action);

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(Hotkeys::GetLabel(action));

	ImGui::TableNextColumn();
	ImGui::PushID(Hotkeys::GetSettingKey(action));

	ImGui::TextUnformatted(keyCapturing ? "press a key"
		: (key != 0 ? GetNameFromVirtualKey(key) : "none"));

	ImGui::SameLine();

	bool needsFunction = Hotkeys::GetKeyNeedsFunction(action);

	if (ImGui::Checkbox("Fn", &needsFunction))
		Hotkeys::SetKey(action, key, needsFunction);

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(padCapturing ? "press a button"
		: PadInput::GetButtonName(Hotkeys::GetPadButton(action)));

	ImGui::TableNextColumn();

	if (ImGui::SmallButton(keyCapturing ? "Cancel" : "Key"))
		SetBindCapture(keyCapturing ? -1 : action, false);

	ImGui::SameLine();

	if (ImGui::SmallButton(padCapturing ? "Cancel##pad" : "Pad"))
		SetBindCapture(padCapturing ? -1 : action, true);

	ImGui::SameLine();

	if (ImGui::SmallButton("Clear"))
	{
		Hotkeys::SetKey(action, 0, false);
		Hotkeys::SetPadButton(action, PadInput::kNone);
	}

	ImGui::PopID();
}

void DrawBindConflicts()
{
	for (int i = 0; i < Hotkeys::Action_Count; ++i)
	{
		const Hotkeys::Action mine = static_cast<Hotkeys::Action>(i);

		for (int k = i + 1; k < Hotkeys::Action_Count; ++k)
		{
			const Hotkeys::Action theirs = static_cast<Hotkeys::Action>(k);

			if (Hotkeys::GetKey(mine) == 0 || Hotkeys::GetKey(mine) != Hotkeys::GetKey(theirs))
				continue;

			if (Hotkeys::GetKeyNeedsFunction(mine) != Hotkeys::GetKeyNeedsFunction(theirs))
				continue;

			ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.45f, 1.0f), "%s and %s are both %s.",
				Hotkeys::GetLabel(mine), Hotkeys::GetLabel(theirs),
				GetNameFromVirtualKey(Hotkeys::GetKey(mine)));
		}
	}
}

}

void ConfigPanel::Draw()
{
	DrawConfigSection();
}
