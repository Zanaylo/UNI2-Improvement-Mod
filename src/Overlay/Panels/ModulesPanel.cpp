#include "Overlay/Panels/ModulesPanel.h"

#include "Core/Config/Features.h"
#include "Overlay/Widgets/UiText.h"
#include "Training/TrainingVisibility.h"

#include <imgui.h>

namespace {

void DrawTrainingItems()
{
	if (!Features::IsOn(FeatureCatalog::Id_Training))
		return;

	ImGui::Indent();

	for (int index = 0; index < TrainingItems::Item_COUNT; ++index)
	{
		const auto item = static_cast<TrainingItems::Item>(index);
		bool shown = TrainingVisibility::IsShown(item);

		ImGui::PushID(index);

		if (ImGui::Checkbox(TrainingItems::Name(item), &shown))
			TrainingVisibility::SetShown(item, shown);

		ImGui::PopID();
	}

	ImGui::Unindent();
}

void DrawModule(FeatureCatalog::Id module)
{
	const FeatureCatalog::Entry& entry = FeatureCatalog::Get(module);
	bool on = Features::IsOn(module);

	ImGui::PushID(module);

	if (ImGui::Checkbox(entry.name, &on))
		Features::Set(module, on);

	UiText::Help(entry.summary);
	ImGui::PopID();

	if (module == FeatureCatalog::Id_Training)
		DrawTrainingItems();
}

}

void ModulesPanel::Draw()
{
	ImGui::Spacing();
	UiText::Muted("A module that is off loads nothing and leaves the menus. A hidden training option is also turned off.");

	for (int id = 0; id < FeatureCatalog::Id_COUNT; ++id)
		DrawModule(static_cast<FeatureCatalog::Id>(id));

	if (Features::NeedsRestart())
		UiText::Warn("Restart the game to apply.");
}
