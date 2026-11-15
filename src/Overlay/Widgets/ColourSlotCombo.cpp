#include "Overlay/Widgets/ColourSlotCombo.h"

#include "Core/Text/Language.h"
#include "Overlay/Widgets/ComboNav.h"
#include "Overlay/Widgets/UiScale.h"
#include "Palette/ColourSlots.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr float kComboWidth = 170.0f;

void ExtendedLabel(int chara, const char* file, char* out, size_t size)
{
	const int extended = ColourSlots::ExtendedOf(chara, file);

	if (extended == ColourSlots::kNoExtended)
	{
		strncpy_s(out, size, "Extended", _TRUNCATE);
		return;
	}

	sprintf_s(out, size, "Extended, EX %d of %d", extended, ColourSlots::ExtendedCount(chara));
}

}

void ColourSlotCombo::Draw(int chara, const char* file)
{
	const int bound = ColourSlots::ColourOf(chara, file);

	char current[160] = {};

	if (bound == ColourSlots::kNoColour)
		ExtendedLabel(chara, file, current, sizeof(current));
	else
		ColourSlots::Label(chara, bound, current, sizeof(current));

	Ui::SetItemWidth(kComboWidth);

	if (ImGui::BeginCombo("##slot", current))
	{
		char label[160] = {};
		ExtendedLabel(chara, file, label, sizeof(label));

		const bool extended = bound == ColourSlots::kNoColour;

		if (ImGui::Selectable(label, extended))
			ColourSlots::Release(chara, file);

		ComboNav::KeepSelectedInView(extended);

		for (int colour = 0; colour < ColourSlots::kGameColours; ++colour)
		{
			const bool selected = colour == bound;

			ColourSlots::Label(chara, colour, label, sizeof(label));
			ImGui::PushID(colour);

			if (ImGui::Selectable(label, selected))
				ColourSlots::Bind(chara, colour, file);

			ComboNav::KeepSelectedInView(selected);
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(Tr("Picking this colour at character select puts this palette on.\n"
			"A palette with no colour is an extended slot: keep pressing right past the game's last "
			"colour to reach it.\nPlayers without the mod see the game's colour - the first one for an "
			"extended slot.\nChanging palette during a match still wins until the next match."));
	}
}
