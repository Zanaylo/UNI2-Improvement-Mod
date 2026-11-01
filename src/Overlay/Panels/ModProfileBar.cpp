#include "Overlay/Panels/ModProfileBar.h"

#include "Core/Config/ConfigProfiles.h"
#include "Core/Text/Language.h"
#include "Core/Text/TextList.h"
#include "Game/Files/ModPacks.h"
#include "Game/Files/ModProfiles.h"
#include "Overlay/Widgets/ComboNav.h"
#include "Overlay/Widgets/UiScale.h"

#include <imgui.h>

namespace {

constexpr float kPickerWidth = 180.0f;
constexpr float kNameWidth = 180.0f;
constexpr int kNoProfile = -1;
constexpr long kStale = -1;

}

bool ModProfileBar::Draw()
{
	Refresh();

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(Tr("Profile"));
	ImGui::SameLine();

	const bool loaded = DrawPicker();

	ImGui::SameLine();
	DrawActions();

	if (m_naming)
		DrawNaming();

	return loaded;
}

void ModProfileBar::Refresh()
{
	if (m_revision == ModPacks::Revision())
		return;

	m_revision = ModPacks::Revision();
	m_names = ModProfiles::List();
	m_active = ModProfiles::Active();
	m_matches = ModProfiles::MatchesActive();
}

bool ModProfileBar::DrawPicker()
{
	const int current = ActiveIndex();
	const std::string preview = m_active.empty() ? Tr("(none)") : m_active + (m_matches ? "" : " *");
	int picked = current;

	Ui::SetItemWidth(kPickerWidth);

	if (ImGui::BeginCombo("##modprofile", preview.c_str()))
	{
		for (int i = 0; i < static_cast<int>(m_names.size()); ++i)
		{
			const bool selected = i == current;

			if (ImGui::Selectable(m_names[i].c_str(), selected))
				picked = i;

			ComboNav::KeepSelectedInView(selected);
		}

		ImGui::EndCombo();
	}

	if (ImGui::IsItemHovered() && !m_active.empty() && !m_matches)
		ImGui::SetTooltip("%s", Tr("The list changed since this profile was saved."));

	if (picked == current)
		picked = ComboNav::WheelTarget(current, static_cast<int>(m_names.size()));

	if (picked == current || picked == kNoProfile || !ModProfiles::Load(m_names[picked]))
		return false;

	m_revision = kStale;
	return true;
}

void ModProfileBar::DrawActions()
{
	ImGui::BeginDisabled(m_active.empty() || m_matches);

	if (ImGui::Button(TrLabel("Save##profile")))
	{
		ModProfiles::Save(m_active);
		m_revision = kStale;
	}

	ImGui::EndDisabled();
	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Save as...")))
		m_naming = !m_naming;

	ImGui::SameLine();
	ImGui::BeginDisabled(m_active.empty());

	if (ImGui::Button(TrLabel("Delete##profile")))
	{
		ModProfiles::Remove(m_active);
		m_revision = kStale;
	}

	ImGui::EndDisabled();
}

void ModProfileBar::DrawNaming()
{
	Ui::SetItemWidth(kNameWidth);
	ImGui::InputTextWithHint("##profilename", Tr("Profile name"), m_name, sizeof(m_name));
	ImGui::SameLine();

	ImGui::BeginDisabled(!ConfigProfiles::IsValidName(m_name));

	if (ImGui::Button(TrLabel("Save##newprofile")) && ModProfiles::Save(m_name))
	{
		m_naming = false;
		m_name[0] = '\0';
		m_revision = kStale;
	}

	ImGui::EndDisabled();
}

int ModProfileBar::ActiveIndex() const
{
	for (int i = 0; i < static_cast<int>(m_names.size()); ++i)
	{
		if (TextList::SameNoCase(m_names[i], m_active))
			return i;
	}

	return kNoProfile;
}
