#include "Overlay/Window/ModsPanel.h"

#include "Game/ModFiles.h"
#include "Game/ModPacks.h"
#include "Game/StageImport.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"

#include <imgui.h>

#include <Windows.h>

#include <cstdio>
#include <string>

namespace {

constexpr float kListHeight = 300.0f;
constexpr float kSwitchColumn = 40.0f;
constexpr float kFilesColumn = 70.0f;
constexpr float kOrderColumn = 100.0f;
constexpr const char* kZipFilter = "A mod (*.zip)\0*.zip\0All files\0*.*\0";

const ImVec4 kOwnText(0.45f, 0.90f, 0.50f, 1.0f);

void Open(const std::string& folder)
{
	ShellExecuteA(nullptr, "open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}

void ModsPanel::Draw()
{
	std::string picked;

	if (m_install.TakeResult(picked) && !picked.empty())
	{
		if (ModPacks::Install(picked, m_status, sizeof(m_status)))
			ModFiles::Rescan();
	}

	if (!ImGui::BeginTabBar("##modstabs"))
		return;

	if (ImGui::BeginTabItem("Mods"))
	{
		DrawTools();
		DrawList();
		DrawFooter();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Help"))
	{
		DrawHelp();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void ModsPanel::DrawTools()
{
	if (ImGui::Button("Install a zip...") && !m_install.IsRunning())
		m_install.BeginOpen("Choose a mod", kZipFilter);

	ImGui::SameLine();

	if (ImGui::Button("Open the folder"))
		Open(ModPacks::Root());

	ImGui::SameLine();

	if (ImGui::Button("Look again"))
	{
		ModPacks::Scan();
		ModFiles::Rescan();
	}

	if (m_status[0] != '\0')
		UiText::Muted("%s", m_status);
}

void ModsPanel::DrawList()
{
	m_moved = -1;
	m_delta = 0;

	if (!ImGui::BeginTable("##mods", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
		ImVec2(0.0f, Ui::Scaled(kListHeight))))
	{
		return;
	}

	ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kSwitchColumn));
	ImGui::TableSetupColumn("Reading order");
	ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kFilesColumn));
	ImGui::TableSetupColumn("Move", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kOrderColumn));
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	DrawOwnRow();

	for (int i = 0; i < ModPacks::Count(); ++i)
		DrawRow(i);

	ImGui::EndTable();

	if (m_moved >= 0)
		m_dirty = ModPacks::Move(m_moved, m_delta) || m_dirty;

	if (!m_dirty)
		return;

	m_dirty = false;
	ModFiles::Rescan();
}

void ModsPanel::DrawOwnRow()
{
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::TextDisabled("--");

	ImGui::TableNextColumn();
	ImGui::TextColored(kOwnText, "Your own files");

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", ModFiles::Root());

	ImGui::SameLine();
	UiText::Muted("always on, always first");

	ImGui::TableNextColumn();
	ImGui::Text("%d", ModFiles::OwnCount());

	ImGui::TableNextColumn();

	if (ImGui::SmallButton("Open"))
		Open(ModFiles::Root());
}

void ModsPanel::DrawRow(int index)
{
	const ModPacks::Pack* const pack = ModPacks::At(index);

	if (pack == nullptr)
		return;

	ImGui::PushID(index);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();

	bool enabled = pack->enabled;

	if (ImGui::Checkbox("##on", &enabled))
	{
		ModPacks::SetEnabled(index, enabled);
		m_dirty = true;
	}

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(pack->name.c_str());

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", pack->path.c_str());

	if (!pack->author.empty() || !pack->version.empty())
	{
		ImGui::SameLine();
		UiText::Muted("%s%s%s", pack->author.c_str(), pack->author.empty() ? "" : " ",
			pack->version.c_str());
	}

	if (!pack->note.empty())
		UiText::Muted("%s", pack->note.c_str());

	if (pack->beaten > 0)
	{
		UiText::Warn("%d file(s) here are already answered by %s, which is higher up.",
			pack->beaten, pack->beatenBy.c_str());
	}

	if (pack->stage > 0)
	{
		const int owner = ModPacks::StageOwner(pack->stage, index);
		const ModPacks::Pack* const other = owner < 0 ? nullptr : ModPacks::At(owner);

		if (other != nullptr)
		{
			UiText::Warn("Stage %d, the same number %s uses. Install one of them and it takes a "
				"free number instead.", pack->stage, other->name.c_str());
		}
		else
		{
			UiText::Muted("Carries stage %d.", pack->stage);
		}

		ImGui::BeginDisabled(StageImport::IsBusy());

		if (ImGui::SmallButton("Install as a stage"))
		{
			char folder[MAX_PATH] = {};
			sprintf_s(folder, "%s\\bg\\bg%03d", pack->path.c_str(), pack->stage);

			const bool started = StageImport::InstallFolder(folder, pack->name.c_str());

			sprintf_s(m_status, "%s", started
				? "installing - the stage list is read at startup, so restart when it finishes"
				: StageImport::StatusText());
		}

		ImGui::EndDisabled();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("A stage has to be registered, not just dropped in, and the game "
				"reads its list once at startup. This copies it to the next free stage number.");
		}
	}

	ImGui::TableNextColumn();
	ImGui::Text("%d", pack->files);

	ImGui::TableNextColumn();
	ImGui::BeginDisabled(index == 0);

	if (ImGui::SmallButton("Up"))
	{
		m_moved = index;
		m_delta = -1;
	}

	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(index + 1 >= ModPacks::Count());

	if (ImGui::SmallButton("Down"))
	{
		m_moved = index;
		m_delta = 1;
	}

	ImGui::EndDisabled();
	ImGui::PopID();
}

void ModsPanel::DrawFooter()
{
	if (ModPacks::Count() == 0)
	{
		UiText::Muted("No mods installed. Install a zip, or drop a mod's folder into %s.",
			ModPacks::Root().c_str());
		return;
	}

	UiText::Good("%d file(s) from %d mod(s) are answering for the game.", ModPacks::FileCount(),
		ModPacks::EnabledCount());
}

void ModsPanel::DrawHelp()
{
	ImGui::TextWrapped("A mod is a folder with the game's own paths inside it. One that replaces "
		"Hyde's voice holds se\\battle_se\\chr000 and nothing else. Install a zip, or drop the "
		"folder in yourself - either way it is in the list within a second, with no restart.");

	ImGui::Spacing();
	ImGui::SeparatorText("What a switch does");

	ImGui::TextWrapped("Switching one off gives the game its own file back the next time it opens "
		"it - the next match, the next screen. Nothing is ever copied over the game and the d "
		"archive is never touched, so removing every mod leaves the install exactly as Steam "
		"put it there.");

	ImGui::Spacing();
	ImGui::SeparatorText("When two mods want the same file");

	ImGui::TextWrapped("The one higher in the list wins that file, and only that file - the rest "
		"of the mod below still applies. A row says so when it happens, naming the mod that beat "
		"it, so Up and Down are how you choose between two mods that both replace Hyde's voice.");

	ImGui::Spacing();
	ImGui::TextWrapped("The whole reading order, first to last:");
	ImGui::BulletText("a loaded game patch");
	ImGui::BulletText("Your own files - the top row, the Mods folder");
	ImGui::BulletText("this list, top to bottom");
	ImGui::BulletText("the game's own d archive, which answers whatever is left");

	ImGui::Spacing();
	ImGui::SeparatorText("Stages are different");

	ImGui::TextWrapped("A stage is not just files: it has to be registered in the game's stage "
		"list, and that list is read once when the game starts. So a mod carrying a stage gets an "
		"Install as a stage button rather than working off the switch. Installing takes the next "
		"free stage number, so two mods built on the same number stop fighting - the second one "
		"lands somewhere else and both are playable.");

	ImGui::Spacing();
	ImGui::SeparatorText("Making one");

	ImGui::TextWrapped("Put your files at the paths the game knows them by, and add a mod.ini so "
		"it has a name. A folder without one still works, listed under its folder name.");

	ImGui::Spacing();
	ImGui::TextUnformatted("[Mod]\nName = Hyde speaks UNI\nAuthor = you\nVersion = 1.0\n"
		"Note = His UNI[st] voice, all 96 lines.");

	ImGui::Spacing();
	ImGui::TextWrapped("The folder is the mod: whoever you send it to has what you have. Your "
		"switches and your ordering stay yours, in your own ini.");

	ImGui::Spacing();
	ImGui::SeparatorText("Online");

	ImGui::TextWrapped("None of this is unloaded when you go online. Art and sound change nothing "
		"anyone else sees, but a mod carrying data or script files is simulation and the other "
		"player will desync.");
}
