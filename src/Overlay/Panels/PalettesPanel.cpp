#include "Overlay/Panels/PalettesPanel.h"

#include "Overlay/Widgets/UiScale.h"

#include "Core/Config/Hotkeys.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/Config/keycodes.h"
#include "Core/Input/KeyboardCapture.h"
#include "Core/Input/PadInput.h"
#include "Core/info.h"
#include "Core/utils.h"
#include "Game/Engine/GameState.h"
#include "Game/Tables/GameTables.h"
#include "Network/PaletteShare.h"
#include "Overlay/Framework/WindowManager.h"
#include "Overlay/Hud/NotificationBar.h"
#include "Overlay/Panels/ColorCustomizePanel.h"
#include "Overlay/Panels/PlayerCardPanel.h"
#include "Overlay/Widgets/ComboNav.h"
#include "Palette/PaletteChoice.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteReport.h"
#include "Palette/PaletteTexture.h"

#include <Windows.h>
#include <imgui.h>
#include <string>

namespace {

constexpr const char* kDefaultPalette = "Default";

ColorCustomizePanel g_colorCustomize;
PlayerCardPanel g_playerCard;

void DrawCustomSection();
void DrawPaletteChoosers();
void DrawPaletteChooser(int player);
void DrawPaletteOptions();
void DrawPalettesTab();
void StepPalette(int player, int applied, int count, int steps);

void DrawCustomSection()
{
	if (!ImGui::CollapsingHeader("Custom"))
		return;

	if (!ImGui::BeginTabBar("##custom"))
		return;

	const bool live = ImGui::BeginTabItem("Palette");

	if (live)
	{
		WindowContainer* const container = WindowManager::GetInstance().GetContainer();
		IWindow* const window = container != nullptr
			? container->GetWindow(WindowType_Palette) : nullptr;

		if (window != nullptr && ImGui::Button(window->IsOpen() ? "Close editor" : "Open editor"))
			window->Toggle();

		DrawPaletteChoosers();
		DrawPaletteOptions();

		ImGui::EndTabItem();
	}

	const bool colorCustomize = ImGui::BeginTabItem("Native Palette");

	if (colorCustomize)
	{
		g_colorCustomize.Draw();
		ImGui::EndTabItem();
	}

	if (g_modVals.showLegacyPalettes && ImGui::BeginTabItem("Palette (legacy)"))
	{
		DrawPalettesTab();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Player Card"))
	{
		g_playerCard.Draw();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void DrawPaletteChoosers()
{
	ImGui::SeparatorText("Character Palette");

	for (int player = 0; player < PaletteControl::kPlayers; ++player)
		DrawPaletteChooser(player);
}

void DrawPaletteChooser(int player)
{
	const int chara = PaletteMemory::GetCharaNumber(player);

	if (chara < 0)
	{
		ImGui::TextDisabled("P%d: no character yet", player + 1);
		return;
	}

	ImGui::PushID(player);

	ImGui::Text("P%d", player + 1);
	ImGui::SameLine();
	ImGui::TextDisabled("%s", PaletteManager::GetCharaName(chara));

	const char* const worn = PaletteChoice::WornFile(player);
	const bool bare = worn[0] == '\0';

	ImGui::BeginDisabled(!PaletteControl::CanEdit(player));

	Ui::SetItemWidth(170.0f);

	if (ImGui::BeginCombo("##worn", bare ? kDefaultPalette : worn))
	{
		if (ImGui::Selectable(kDefaultPalette, bare))
			PaletteChoice::Bare(player);

		ComboNav::KeepSelectedInView(bare);

		for (int i = 0; i < PaletteLibrary::GetCount(chara); ++i)
		{
			const char* const file = PaletteLibrary::GetName(chara, i);
			const bool selected = !bare && strcmp(file, worn) == 0;

			ImGui::PushID(i);

			if (ImGui::Selectable(file, selected))
				PaletteChoice::Wear(player, file);

			ComboNav::KeepSelectedInView(selected);

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	const int steps = ComboNav::WheelSteps();

	if (steps != 0)
		PaletteChoice::Step(player, steps);

	ImGui::SameLine();

	if (ImGui::Button("Rescan"))
		PaletteLibrary::Rescan(chara);

	ImGui::EndDisabled();

	ImGui::PopID();
}

void DrawPaletteOptions()
{
	if (ImGui::Checkbox("Group by part", &g_modVals.paletteGroupByPart))
		Settings::SaveInt("Palette", "GroupByPart", g_modVals.paletteGroupByPart ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Groups the colours by part (hair, skin, boots), the same way the game's "
			"own colour screen does.");
	}

	if (ImGui::Checkbox("Flash the entry on the character", &g_modVals.paletteFlashEntry))
		Settings::SaveInt("Palette", "FlashEntry", g_modVals.paletteFlashEntry ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("When you pick a colour, everything else darkens and that part blinks on "
			"the character, so you can see what you are about to change.");
	}

	if (ImGui::Checkbox("Filter junk colours", &g_modVals.paletteFilterJunk))
		Settings::SaveInt("Palette", "FilterJunk", g_modVals.paletteFilterJunk ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Hides the colours this character's sprites never use, and the green that "
			"fills unused slots. Colours the game's own colour screen offers are always kept.");
	}

	if (ImGui::Checkbox("See the other player's colours", &g_modVals.showOnlinePalettes))
		Settings::SaveInt("Palette", "ShowOnlinePalettes", g_modVals.showOnlinePalettes ? 1 : 0);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Turn this on to see the palette your opponent picked. When off, their "
			"character keeps the game's colours. Yours is sent either way.");
	}

	ImGui::TextDisabled("%s", PaletteControl::IsSpectating()
		? "watching: the colours are the players' own"
		: (PaletteControl::LocalPlayer() >= 0
			? (PaletteControl::LocalPlayer() == 0 ? "you are playing P1" : "you are playing P2")
			: "both characters are yours to dress"));
}

void DrawPalettesTab()
{
	if (!GameState::AllowsPalettes())
	{
		ImGui::TextDisabled("In a match only.");
		return;
	}

	if (ImGui::Button("Load Palettes"))
		PaletteManager::Refresh();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Reloads UNI2-IM\\Palettes. Each character has a folder named after them. "
			"Put a .pal file there (the game's format, which Hantei-kun also saves) and it shows up "
			"here. The folders are created the first time you press this.");
	}

	ImGui::SameLine();

	WindowContainer* palettes = WindowManager::GetInstance().GetContainer();
	IWindow* editor = palettes != nullptr ? palettes->GetWindow(WindowType_PaletteEditor)
		: nullptr;

	if (editor != nullptr && ImGui::Button(editor->IsOpen() ? "Close editor" : "Open editor"))
		editor->Toggle();

	ImGui::SameLine();

	if (ImGui::Button("Log palettes"))
	{
		std::string path;

		if (PaletteReport::Write(path))
			NotificationBar::Add("Palette report written to %s", path.c_str());
		else
			NotificationBar::Add("Could not write the palette report");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Writes the palette state to a text file in UNI2-IM\\Logs: which side the "
			"mod thinks you are, what each side is wearing, the textures it tracks and what was "
			"drawn.\n\n"
			"If a shared palette looks wrong, take one on each PC at the same moment. Works in "
			"this build, no logging build needed.");
	}

	ImGui::TextDisabled("(%s)", PaletteShare::GetStatusText());

	for (int player = 0; player < 2; ++player)
	{
		ImGui::PushID(player);

		const int chara = PaletteManager::GetCharaNumber(player);
		const int count = PaletteManager::GetCount(player);
		const int applied = PaletteManager::GetApplied(player);

		ImGui::Text("P%d  %s", player + 1, PaletteManager::GetCharaName(chara));

		if (PaletteTexture::FindForPlayer(player) < 0)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(no palette texture for this side yet)");
			ImGui::PopID();
			continue;
		}

		if (count == 0)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(no palettes in its folder)");
			ImGui::PopID();
			continue;
		}

		ImGui::SameLine();
		Ui::SetItemWidth(220.0f);

		int chosen = applied + 1;

		const char* const worn = GameTables::PaletteName(chara,
			PaletteMemory::GetPlayerSlot(player));

		if (ImGui::BeginCombo("##palette", chosen == 0 ? "Default"
			: PaletteManager::GetName(player, applied)))
		{
			if (ImGui::Selectable("Default", chosen == 0))
				PaletteManager::Restore(player);

			if (ImGui::IsItemHovered() && worn[0] != '\0')
				ImGui::SetTooltip("%s: the colours this character was picked with", worn);

			ComboNav::KeepSelectedInView(chosen == 0);

			for (int i = 0; i < count; ++i)
			{
				const bool selected = i == applied;

				ImGui::PushID(i);

				if (ImGui::Selectable(PaletteManager::GetName(player, i), selected))
					PaletteManager::Apply(player, i);

				const char* creator = PaletteManager::GetCreator(player, i);
				if (ImGui::IsItemHovered() && creator[0] != '\0')
					ImGui::SetTooltip("by %s", creator);

				ComboNav::KeepSelectedInView(selected);

				ImGui::PopID();
			}

			ImGui::EndCombo();
		}

		StepPalette(player, applied, count, ComboNav::WheelSteps());

		ImGui::PopID();
	}
}

void StepPalette(int player, int applied, int count, int steps)
{
	if (steps == 0 || count == 0)
		return;

	int target = applied + steps;

	if (target < -1)
		target = -1;

	if (target >= count)
		target = count - 1;

	if (target == applied)
		return;

	if (target < 0)
	{
		PaletteManager::Restore(player);
		return;
	}

	PaletteManager::Apply(player, target);
}

}

void PalettesPanel::Draw()
{
	DrawCustomSection();
}
