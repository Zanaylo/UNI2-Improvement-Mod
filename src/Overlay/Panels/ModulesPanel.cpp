#include "Overlay/Panels/ModulesPanel.h"

#include "Addons/AddonManager.h"
#include "Core/Config/Features.h"
#include "Core/Text/Language.h"
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

void DrawAddons()
{
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::TextUnformatted(Tr("Add-ons"));
	UiText::Muted("Installed in UNI2-IM\\Addons. Lua add-ons toggle right away; native DLL add-ons apply after restart.");

	const auto& addons = AddonManager::All();

	if (addons.empty())
	{
		UiText::Muted("No add-ons installed.");
		return;
	}

	for (size_t i = 0; i < addons.size(); ++i)
	{
		const AddonManager::Record& record = addons[i];
		bool on = record.enabled;

		ImGui::PushID(1000 + static_cast<int>(i));

		if (ImGui::Checkbox(record.manifest.name.c_str(), &on))
			AddonManager::SetEnabled(record.manifest.id, on);

		if (!record.manifest.description.empty())
			UiText::Help(record.manifest.description.c_str());

		if (!record.error.empty())
			UiText::Warn("%s", record.error.c_str());

		ImGui::PopID();
	}
}

}

void ModulesPanel::Draw()
{
	ImGui::Spacing();
	UiText::Muted("A module that is off loads nothing and leaves the menus. A hidden training option is also turned off.");

	for (int id = 0; id < FeatureCatalog::Id_COUNT; ++id)
	{
		const auto feature = static_cast<FeatureCatalog::Id>(id);

		if (Features::IsAvailable(feature))
			DrawModule(feature);
	}

	DrawAddons();

	if (Features::NeedsRestart() || AddonManager::NeedsRestart())
		UiText::Warn("Restart the game to apply.");
}
