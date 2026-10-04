#include "Overlay/Panels/MiscPanel.h"

#include "Game/Audio/AnnouncerImport.h"
#include "Game/Audio/AnnouncerRoster.h"
#include "Game/Customize/PortraitImport.h"
#include "Game/Customize/PortraitLayer.h"
#include "Game/Tables/CharaTables.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace {

constexpr int kPortraitColumns = 3;
constexpr int kAnnouncerColumns = 4;

}

void MiscPanel::Draw()
{
	std::string picked;

	if (m_portraits.TakeResult(picked) && !picked.empty())
		PortraitImport::Begin(picked.c_str());

	if (m_announcers.TakeResult(picked) && !picked.empty())
		AnnouncerImport::Begin(picked.c_str());

	if (!ImGui::BeginTabBar("##misctabs"))
		return;

	if (ImGui::BeginTabItem("Old portraits"))
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
	UiText::Muted("Puts the old art of UNDER NIGHT IN-BIRTH Exe:Late[st] back on character select, the "
		"versus screen, the battle gauge, the winner screen and the main menu. Tick the characters that "
		"wear it.");

	ImGui::Spacing();
	ImGui::BeginDisabled(PortraitImport::IsBusy());

	if (ImGui::Button(PortraitImport::IsInstalled() ? "Read them again" : "Get the old portraits")
		&& !m_portraits.IsRunning())
	{
		const std::string found = PortraitImport::FindGame();

		if (found.empty())
			m_portraits.BeginFolder("Pick the UNDER NIGHT IN-BIRTH Exe:Late[st] folder");
		else
			PortraitImport::Begin(found.c_str());
	}

	ImGui::EndDisabled();

	ImGui::SameLine();
	UiText::Help("Reads the art from your copy of the game, once. Each screen shows a change the next time "
		"it opens.");

	if (PortraitImport::IsBusy())
	{
		ImGui::ProgressBar(PortraitImport::Progress() / 100.0f, Ui::Scaled(260.0f, 0.0f));
		UiText::Warn("%s", PortraitImport::StatusText().c_str());
		return;
	}

	if (PortraitImport::Progress() > 0)
		UiText::Muted("%s", PortraitImport::StatusText().c_str());

	DrawPortraitPicks();
}

void MiscPanel::DrawPortraitPicks()
{
	const std::vector<int> available = PortraitLayer::Available();

	if (available.empty())
		return;

	ImGui::Spacing();

	if (ImGui::SmallButton("All"))
		PortraitLayer::WearAll(true);

	ImGui::SameLine();

	if (ImGui::SmallButton("None"))
		PortraitLayer::WearAll(false);

	if (!ImGui::BeginTable("##portraits", kPortraitColumns, ImGuiTableFlags_SizingStretchSame))
		return;

	for (int chara : available)
	{
		ImGui::TableNextColumn();
		ImGui::PushID(chara);

		bool old = PortraitLayer::IsWorn(chara);

		if (ImGui::Checkbox(CharaTables::Name(chara), &old))
			PortraitLayer::Wear(chara, old);

		ImGui::PopID();
	}

	ImGui::EndTable();
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
