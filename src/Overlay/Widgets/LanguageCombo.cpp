#include "Overlay/Widgets/LanguageCombo.h"

#include "Core/Text/Language.h"
#include "Overlay/Widgets/ComboNav.h"
#include "Overlay/Widgets/UiScale.h"

#include <imgui.h>

#include <Windows.h>

#include <cstdio>

namespace {

using LanguagePick::Choice;

constexpr float kComboWidth = 220.0f;

const char* ShownName(Choice choice)
{
	if (choice != Choice::Automatic)
		return LanguagePick::NameOf(choice);

	static char automatic[96] = {};
	const LanguageId system = LanguagePick::FromSystem(GetUserDefaultUILanguage());
	snprintf(automatic, sizeof(automatic), "%s (%s)", Tr("Automatic"),
		LanguagePick::NameOf(LanguagePick::ChoiceOf(system)));
	return automatic;
}

void DrawChoices(Choice chosen)
{
	for (int index = 0; index < LanguagePick::kChoiceCount; ++index)
	{
		const Choice choice = static_cast<Choice>(index);
		const bool selected = choice == chosen;

		if (ImGui::Selectable(ShownName(choice), selected))
			Language::Choose(choice);

		ComboNav::KeepSelectedInView(selected);
	}
}

}

void LanguageCombo::Draw()
{
	const Choice chosen = Language::Chosen();

	Ui::SetItemWidth(kComboWidth);

	if (ImGui::BeginCombo(TrLabel("Language##modLanguage"), ShownName(chosen)))
	{
		DrawChoices(chosen);
		ImGui::EndCombo();
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", Tr("The language of the mod's windows. Automatic follows Windows."));

	const int index = static_cast<int>(chosen);
	const int target = ComboNav::WheelTarget(index, LanguagePick::kChoiceCount);

	if (target != index)
		Language::Choose(static_cast<Choice>(target));
}
