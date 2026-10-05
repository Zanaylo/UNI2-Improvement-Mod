#include "Overlay/Panels/MiscPanel.h"

#include "Game/Audio/AnnouncerImport.h"
#include "Game/Audio/AnnouncerRoster.h"
#include "Game/Customize/PortraitCatalog.h"
#include "Game/Customize/PortraitChoices.h"
#include "Game/Customize/PortraitCompose.h"
#include "Game/Customize/PortraitDownload.h"
#include "Game/Customize/PortraitLibrary.h"
#include "Game/Customize/PortraitStyles.h"
#include "Game/Tables/CharaTables.h"
#include "Overlay/Widgets/ComboNav.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int kPortraitColumns = 2;
constexpr int kLastFighter = 27;
constexpr const char* kGameArt = "Game's own";
constexpr const char* kToDownload = "  (download)";
constexpr const char* kEveryoneHint = "Pick one style";
constexpr int kGameOption = 0;
constexpr int kNoArt = -1;

const char* StyleLabel(int option)
{
	return option == kGameOption ? kGameArt : PortraitStyles::Name(option - 1);
}

int IndexOf(const std::vector<const PortraitCatalog::Art*>& arts, const PortraitCatalog::Art* worn)
{
	const auto found = std::find(arts.begin(), arts.end(), worn);
	return found == arts.end() ? kNoArt : static_cast<int>(found - arts.begin());
}
constexpr int kAnnouncerColumns = 4;

}

void MiscPanel::Draw()
{
	std::string picked;

	if (m_announcers.TakeResult(picked) && !picked.empty())
		AnnouncerImport::Begin(picked.c_str());

	if (!ImGui::BeginTabBar("##misctabs"))
		return;

	if (ImGui::BeginTabItem("Portraits"))
	{
		DrawPortraits();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("MBTL announcers"))
	{
		DrawAnnouncers();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void MiscPanel::DrawPortraits()
{
	if (!m_portraitsSeen)
	{
		m_portraitsSeen = true;
		PortraitCompose::Refresh();
	}

	UiText::Muted("Pick the art each character wears on character select, the versus screen, the battle gauge, "
		"the winner screen and the main menu.");

	ImGui::Spacing();
	DrawPortraitDownload();
	DrawPortraitStatus();
	DrawPortraitPicks();
}

void MiscPanel::DrawPortraitDownload()
{
	const bool linked = PortraitDownload::HasLink();

	ImGui::BeginDisabled(PortraitDownload::IsBusy() || !linked);

	if (ImGui::Button("Download the portrait pack"))
		PortraitDownload::BeginAll();

	ImGui::EndDisabled();

	ImGui::SameLine();
	UiText::Help(linked ? "Fetches every art that is not in the game, about 120 MB, once. Picking one of them fetches "
		"the pack too." : "No link to the portrait pack is set yet. Put it in [Portraits] PackUrl of UNI2_IM.ini.");

	if (PortraitDownload::IsBusy())
	{
		ImGui::ProgressBar(PortraitDownload::Progress() / 100.0f, Ui::Scaled(260.0f, 0.0f));
		UiText::Warn("%s", PortraitDownload::StatusText().c_str());
		return;
	}

	if (PortraitDownload::Progress() > 0)
		UiText::Muted("%s", PortraitDownload::StatusText().c_str());
}

void MiscPanel::DrawPortraitStatus()
{
	if (PortraitCompose::IsBusy())
	{
		ImGui::ProgressBar(PortraitCompose::Progress() / 100.0f, Ui::Scaled(260.0f, 0.0f));
		UiText::Warn("%s", PortraitCompose::StatusText().c_str());
		return;
	}

	if (PortraitCompose::Progress() > 0)
		UiText::Muted("%s", PortraitCompose::StatusText().c_str());
}

void MiscPanel::DrawPortraitPicks()
{
	ImGui::Spacing();
	DrawPortraitStyle();
	ImGui::Spacing();

	if (!ImGui::BeginTable("##portraits", kPortraitColumns, ImGuiTableFlags_SizingStretchSame))
		return;

	for (int chara = 0; chara <= kLastFighter; ++chara)
	{
		ImGui::TableNextColumn();
		ImGui::PushID(chara);
		DrawPortraitPick(chara);
		ImGui::PopID();
	}

	ImGui::EndTable();
}

void MiscPanel::DrawPortraitStyle()
{
	const int options = PortraitStyles::Count() + 1;

	if (ImGui::BeginCombo("Everyone", m_portraitStyle < 0 ? kEveryoneHint : StyleLabel(m_portraitStyle)))
	{
		for (int option = 0; option < options; ++option)
		{
			const bool selected = option == m_portraitStyle;

			if (ImGui::Selectable(StyleLabel(option), selected))
				WearStyle(option);

			ComboNav::KeepSelectedInView(selected);
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();
	UiText::Help("Gives every character the same kind of art. A character without it wears the game's own.");

	const int steps = ComboNav::WheelSteps();

	if (steps == 0)
		return;

	const int target = std::clamp(m_portraitStyle + steps, 0, options - 1);

	if (target != m_portraitStyle)
		WearStyle(target);
}

void MiscPanel::DrawPortraitPick(int chara)
{
	const std::vector<const PortraitCatalog::Art*> arts = PortraitCatalog::Of(chara);
	const PortraitCatalog::Art* const worn = PortraitCatalog::Find(PortraitChoices::Of(chara));

	if (ImGui::BeginCombo(CharaTables::Name(chara), worn == nullptr ? kGameArt : worn->label))
	{
		if (ImGui::Selectable(kGameArt, worn == nullptr))
			PortraitCompose::Wear(chara, std::string());

		ComboNav::KeepSelectedInView(worn == nullptr);

		for (const PortraitCatalog::Art* art : arts)
		{
			const std::string label = std::string(art->label) + (PortraitLibrary::IsReady(*art) ? "" : kToDownload);

			ImGui::PushID(art->id);

			if (ImGui::Selectable(label.c_str(), art == worn))
				PortraitCompose::Wear(chara, art->id);

			ComboNav::KeepSelectedInView(art == worn);
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	const int steps = ComboNav::WheelSteps();

	if (steps == 0)
		return;

	const int target = std::clamp(IndexOf(arts, worn) + steps, kNoArt, static_cast<int>(arts.size()) - 1);
	PortraitCompose::Wear(chara, target == kNoArt ? std::string() : arts[static_cast<size_t>(target)]->id);
}

void MiscPanel::WearStyle(int option)
{
	m_portraitStyle = option;

	for (int chara = 0; chara <= kLastFighter; ++chara)
	{
		const PortraitCatalog::Art* const art = PortraitStyles::For(chara, option - 1);
		PortraitCompose::Wear(chara, art == nullptr ? std::string() : art->id);
	}
}

void MiscPanel::DrawAnnouncers()
{
	UiText::Muted("Takes the announcers from MELTY BLOOD TYPE LUMINA and puts the ticked ones in Customize, "
		"Announcer Character, each with its own icon.");

	ImGui::Spacing();
	ImGui::BeginDisabled(AnnouncerImport::IsBusy());

	if (ImGui::Button("Get the MBTL announcers") && !m_announcers.IsRunning())
	{
		const std::string found = AnnouncerImport::FindGame();

		if (found.empty())
			m_announcers.BeginFolder("Pick the MELTY BLOOD TYPE LUMINA folder");
		else
			AnnouncerImport::Begin(found.c_str());
	}

	ImGui::EndDisabled();

	ImGui::SameLine();
	UiText::Help("The menu has room for a limited number. Change the ticks and press the button again to "
		"swap them. The game has to be restarted before they show up.");

	DrawAnnouncerPicks();

	if (AnnouncerImport::IsBusy())
	{
		ImGui::ProgressBar(AnnouncerImport::Progress() / 100.0f, Ui::Scaled(260.0f, 0.0f));
		UiText::Warn("%s", AnnouncerImport::StatusText().c_str());
		return;
	}

	if (AnnouncerImport::Progress() > 0)
		UiText::Muted("%s", AnnouncerImport::StatusText().c_str());
}

void MiscPanel::DrawAnnouncerPicks()
{
	const int capacity = AnnouncerRoster::Capacity();
	const int chosen = AnnouncerRoster::ChosenCount();

	ImGui::Spacing();
	UiText::Muted("Shown in the menu: %d of %d", chosen, capacity);

	if (!ImGui::BeginTable("##announcers", kAnnouncerColumns, ImGuiTableFlags_SizingStretchSame))
		return;

	for (int i = 0; i < AnnouncerRoster::Count(); ++i)
	{
		const AnnouncerRoster::Speaker& speaker = AnnouncerRoster::At(i);
		bool ticked = AnnouncerRoster::IsChosen(speaker.folder);

		ImGui::TableNextColumn();
		ImGui::BeginDisabled(!ticked && chosen >= capacity);

		if (ImGui::Checkbox(speaker.name, &ticked))
			AnnouncerRoster::SetChosen(speaker.folder, ticked);

		ImGui::EndDisabled();
	}

	ImGui::EndTable();
}
