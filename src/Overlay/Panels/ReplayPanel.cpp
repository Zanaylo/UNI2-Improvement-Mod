#include "Overlay/Panels/ReplayPanel.h"

#include "Overlay/Widgets/UiScale.h"

#include "Core/Config/Hotkeys.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/Config/keycodes.h"
#include "Core/Input/KeyboardCapture.h"
#include "Core/Input/PadInput.h"
#include "Core/info.h"
#include "Core/utils.h"
#include "Game/Lobby/SteamNames.h"
#include "Game/Patches/GamePatches.h"
#include "Game/Replays/ReplayFiles.h"
#include "Overlay/Hud/NotificationBar.h"
#include "Overlay/Widgets/ComboNav.h"
#include "Overlay/Widgets/UiText.h"

#include <Windows.h>
#include <imgui.h>
#include <string>

namespace {

bool Contains(const std::string& text, const char* needle)
{
	if (needle == nullptr || needle[0] == '\0')
		return true;

	const auto fold = [](char c) { return static_cast<char>(tolower(static_cast<unsigned char>(c))); };

	const size_t length = strlen(needle);
	if (length > text.size())
		return false;

	for (size_t at = 0; at + length <= text.size(); ++at)
	{
		size_t i = 0;
		while (i < length && fold(text[at + i]) == fold(needle[i]))
			++i;

		if (i == length)
			return true;
	}

	return false;
}

void DrawReplayPatchWarning();
void DrawReplayAccounts();
void DrawReplaySection();

void DrawReplayPatchWarning()
{
	const int wanted = GamePatches::ReplayWanted();

	if (wanted < 0 || GamePatches::TablesAgreeWith(wanted))
		return;

	const GamePatches::Patch* const needs = GamePatches::Get(wanted);

	if (needs == nullptr)
		return;

	UiText::Muted("The last replay needs %s. Start Replay loads it for you and plays.",
		needs->name.c_str());
}

void DrawReplayAccounts()
{
	const int accounts = ReplayFiles::AccountCount();

	if (accounts < 2)
		return;

	const int selected = ReplayFiles::SelectedAccount();

	Ui::SetItemWidth(300.0f);

	if (ImGui::BeginCombo("Save folder", ReplayFiles::AccountLabel(selected).c_str()))
	{
		for (int i = 0; i < accounts; ++i)
		{
			if (ImGui::Selectable(ReplayFiles::AccountLabel(i).c_str(), i == selected))
				ReplayFiles::SelectAccount(i);
		}

		ImGui::EndCombo();
	}

	UiText::Help("This install has saves from more than one Steam account. Export all uses the one "
		"picked here. Other accounts are read only, and loading a file into the replay list always "
		"writes to yours.");

	if (!ReplayFiles::IsOwnAccount())
	{
		UiText::Warn("Reading another account's replays. Nothing is written to it, and new matches "
			"still save to yours.");
	}

	ImGui::Spacing();
}

void DrawReplaySection()
{
	if (!ImGui::CollapsingHeader("Replays"))
		return;

	const bool readable = ReplayFiles::IsAvailable();
	const bool live = ReplayFiles::IsLive();

	ImGui::TextWrapped("Each new replay is saved to UNI2-IM\\Replays as its own file, so you can "
		"share a single match. Files are named after the two players, or P1 and P2 when Steam "
		"cannot find a name. Press Export all to also save the replays already in REP-DATA.");

	ImGui::Spacing();

	bool automatic = ReplayFiles::GetAutoExport();
	if (ImGui::Checkbox("Save each new replay to a file", &automatic))
	{
		ReplayFiles::SetAutoExport(automatic);
		Settings::SaveInt("Replays", "AutoExport", automatic ? 1 : 0);
	}

	DrawReplayPatchWarning();
	DrawReplayAccounts();

	ImGui::BeginDisabled(!readable);

	if (ImGui::Button("Export all"))
	{
		std::string error;
		ReplayFiles::ExportAll(error);
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Anything already in the folder is left alone.");

	ImGui::EndDisabled();

	ImGui::SameLine();

	if (ImGui::Button("Open folder"))
		ShellExecuteA(nullptr, "open", ReplayFiles::GetFolder().c_str(), nullptr, nullptr, SW_SHOWNORMAL);

	ImGui::SameLine();

	if (ImGui::Button("Refresh"))
		ReplayFiles::Refresh();

	ImGui::Spacing();

	const std::vector<std::string>& files = ReplayFiles::ListFiles();
	static int picked = 0;

	if (picked >= static_cast<int>(files.size()))
		picked = 0;

	if (files.empty())
	{
		ImGui::TextDisabled("No replay files yet.");
	}
	else
	{
		static char search[64] = "";

		Ui::SetItemWidth(-1.0f);

		if (ImGui::BeginCombo("##replayfile", files[picked].c_str()))
		{
			if (ImGui::IsWindowAppearing())
			{
				search[0] = '\0';
				ImGui::SetKeyboardFocusHere();
			}

			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##replaysearch", "Search", search, sizeof(search));

			ImGui::Separator();

			for (int i = 0; i < static_cast<int>(files.size()); ++i)
			{
				if (!Contains(files[i], search))
					continue;

				const bool selected = i == picked;

				ImGui::PushID(i);

				if (ImGui::Selectable(files[i].c_str(), selected))
					picked = i;

				ComboNav::KeepSelectedInView(selected);

				ImGui::PopID();
			}

			ImGui::EndCombo();
		}

		const int steps = ComboNav::WheelSteps();
		const int target = picked + steps;

		if (steps != 0 && target >= 0 && target < static_cast<int>(files.size()))
			picked = target;

		const bool canPlay = ReplayFiles::CanPlay();

		ImGui::BeginDisabled(!canPlay);

		if (ImGui::Button("Start Replay"))
		{
			std::string error;
			if (!ReplayFiles::RequestPlayback(ReplayFiles::GetFolder() + files[picked], error))
				NotificationBar::Add("%s", error.c_str());
		}

		ImGui::EndDisabled();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(canPlay
				? "Plays this file right away, the same way the game's Replay list does, with "
				  "both player names. It does not use a slot or change REP-DATA."
				: "Not while a match is running.");
		}

		ImGui::SameLine();

		ImGui::BeginDisabled(!readable);

		if (ImGui::Button("Load into the game's replay list"))
		{
			std::string error;
			if (!ReplayFiles::Import(ReplayFiles::GetFolder() + files[picked], error))
				NotificationBar::Add("%s", error.c_str());
		}

		ImGui::EndDisabled();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Writes it over the oldest unprotected slot. It shows up in the Replay "
				"screen under its own date. Protect any replay you want to keep first.");
		}
	}

	if (!readable)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("Could not read any replays, from the game or from REP-DATA.");
		return;
	}

	ImGui::Spacing();
	ImGui::TextDisabled("%d of %d slots used, replay format version %d%s",
		ReplayFiles::CountUsed(), ReplayFiles::kSlotCount, ReplayFiles::CurrentVersion(),
		live ? "" : " (read from REP-DATA, loaded files show up after a restart)");

	ImGui::TextDisabled("Steam names: %s", SteamNames::GetStatus());

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Names are saved in UNI2-IM\\SteamNames.txt and reused every session. If "
			"Steam cannot find a name, it asks again at most once every 20 seconds.");
	}

	if (ReplayFiles::GetStatus()[0] != 0)
		ImGui::TextDisabled("%s", ReplayFiles::GetStatus());

}

}

void ReplayPanel::Draw()
{
	DrawReplaySection();
}
