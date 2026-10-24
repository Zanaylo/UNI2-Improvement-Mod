#include "Overlay/Panels/ProfilesPanel.h"

#include "Core/Config/ConfigProfiles.h"
#include "Core/Config/ProfileStore.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace {

constexpr float kNameWidth = 220.0f;
constexpr size_t kNameBytes = 33;

char g_name[kNameBytes] = {};
std::vector<std::string> g_profiles;
bool g_listed = false;

void Refresh()
{
	g_profiles = ProfileStore::List();
	g_listed = true;
}

void DrawSave()
{
	Ui::SetItemWidth(kNameWidth);
	ImGui::InputTextWithHint("##profile", "Profile name", g_name, sizeof(g_name));

	const bool valid = ConfigProfiles::IsValidName(g_name);

	ImGui::SameLine();
	ImGui::BeginDisabled(!valid);

	if (ImGui::Button("Save current settings") && ProfileStore::Save(g_name))
		Refresh();

	ImGui::EndDisabled();

	UiText::Help("Saves every setting as it is now under this name. Letters, digits, spaces, - and _, up to 32.");
}

void DrawProfile(const std::string& name)
{
	ImGui::PushID(name.c_str());
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(name.c_str());
	ImGui::SameLine();

	if (ImGui::SmallButton("Load"))
		ProfileStore::Load(name);

	ImGui::SameLine();

	if (ImGui::SmallButton("Delete") && ProfileStore::Delete(name))
		Refresh();

	ImGui::PopID();
}

}

void ProfilesPanel::Draw()
{
	if (!g_listed)
		Refresh();

	ImGui::Spacing();

	DrawSave();
	ImGui::Separator();

	if (g_profiles.empty())
		UiText::Muted("No profile saved yet.");

	for (const std::string& name : g_profiles)
		DrawProfile(name);

	if (ProfileStore::NeedsRestart())
		UiText::Warn("Restart the game to use the loaded profile.");
}
