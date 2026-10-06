#include "Overlay/Panels/NetworkPalettePanel.h"

#include "Core/Config/interfaces.h"
#include "Game/Tables/CharaTables.h"
#include "Overlay/Widgets/ComboNav.h"
#include "Overlay/Widgets/UiScale.h"
#include "Palette/ColourSlots.h"
#include "Palette/NetworkPick.h"
#include "Palette/PaletteCycle.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/RoomPalette.h"

#include <imgui.h>

#include <cstdio>

namespace {

constexpr float kComboWidth = 240.0f;
constexpr int kNextStep = 1;
constexpr int kPreviousStep = -1;

void GameColourLabel(const NetworkPick::Pick& pick, char* out, size_t size)
{
	const char* const bound = ColourSlots::BoundFile(pick.chara, pick.colour);

	if (bound[0] == '\0')
	{
		sprintf_s(out, size, "Game colour %03d", pick.colour + 1);
		return;
	}

	sprintf_s(out, size, "Colour %03d - %s", pick.colour + 1, bound);
}

void DrawChoices(const NetworkPick::Pick& pick)
{
	const int current = RoomPalette::Index();

	char gameColour[160] = {};
	GameColourLabel(pick, gameColour, sizeof(gameColour));

	Ui::SetItemWidth(kComboWidth);

	if (!ImGui::BeginCombo("##networkpalette", current == PaletteCycle::kNone ? gameColour :
		PaletteLibrary::GetName(pick.chara, current)))
	{
		return;
	}

	if (ImGui::Selectable(gameColour, current == PaletteCycle::kNone))
		RoomPalette::Choose("");

	ComboNav::KeepSelectedInView(current == PaletteCycle::kNone);

	for (int i = 0; i < PaletteLibrary::GetCount(pick.chara); ++i)
	{
		ImGui::PushID(i);

		if (ImGui::Selectable(PaletteLibrary::GetName(pick.chara, i), i == current))
			RoomPalette::Choose(PaletteLibrary::GetName(pick.chara, i));

		ComboNav::KeepSelectedInView(i == current);
		ImGui::PopID();
	}

	ImGui::EndCombo();
}

void DrawSteps(int chara)
{
	ImGui::SameLine();

	if (ImGui::ArrowButton("##previous", ImGuiDir_Left))
		RoomPalette::Step(kPreviousStep);

	ImGui::SameLine();

	if (ImGui::ArrowButton("##next", ImGuiDir_Right))
		RoomPalette::Step(kNextStep);

	ImGui::SameLine();

	if (ImGui::Button("Rescan"))
		PaletteLibrary::Rescan(chara);
}

void DrawNotes(const NetworkPick::Pick& pick)
{
	ImGui::TextWrapped("Worn by your room avatar and in your next online match. The next and previous "
		"palette hotkeys change it too.");

	if (!pick.hasSignature)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Confirm this colour once at Network > Character "
			"Select so the room can recognise your avatar.");
	}

	if (!g_modVals.sharePalettes)
		ImGui::TextDisabled("Sharing palettes is off, so other players see the game's colour.");
}

}

void NetworkPalettePanel::Draw()
{
	ImGui::SeparatorText("Network palette");

	NetworkPick::Pick pick = {};

	if (!NetworkPick::Recall(pick))
	{
		ImGui::TextWrapped("Pick a character at Network > Character Select first.");
		return;
	}

	ImGui::Text("%s, colour %03d", CharaTables::Name(pick.chara), pick.colour + 1);

	DrawChoices(pick);
	DrawSteps(pick.chara);
	DrawNotes(pick);
}
