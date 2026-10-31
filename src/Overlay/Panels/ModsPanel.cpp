#include "Overlay/Panels/ModsPanel.h"

#include "Core/ShellOpen.h"
#include "Game/Audio/BgmOverride.h"
#include "Game/Files/ModFiles.h"
#include "Game/Files/ModPacks.h"
#include "Game/Stages/StageImport.h"
#include "Game/Stages/StageLibrary.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"
#include "Overlay/Panels/RestartPrompt.h"
#include "Core/Text/Language.h"

#include <imgui.h>

#include <Windows.h>

#include <cstdio>
#include <string>

namespace {

constexpr float kListHeight = 300.0f;
constexpr float kSwitchColumn = 40.0f;
constexpr float kFilesColumn = 70.0f;
constexpr float kOrderColumn = 100.0f;
constexpr size_t kStageListBytes = 256;
constexpr const char* kZipFilter = "A mod (*.zip)\0*.zip\0All files\0*.*\0";

const ImVec4 kUserText(0.45f, 0.90f, 0.50f, 1.0f);

void Open(const std::string& folder)
{
	ShellOpen::Open(folder);
}

bool IsNewStage(int number)
{
	return number <= StageLibrary::kSlotLast && !StageLibrary::IsGameStage(number);
}

}

void ModsPanel::Draw()
{
	std::string picked;

	if (m_install.TakeResult(picked) && !picked.empty())
	{
		if (ModPacks::Install(picked, m_status, sizeof(m_status)))
		{
			ModFiles::Rescan();
			m_changed = true;
		}
	}

	if (!ImGui::BeginTabBar("##modstabs"))
		return;

	if (ImGui::BeginTabItem(TrLabel("Mods")))
	{
		DrawTools();
		DrawList();
		DrawFooter();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem(TrLabel("Help")))
	{
		DrawHelp();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void ModsPanel::DrawTools()
{
	if (ImGui::Button(TrLabel("Install a zip...")) && !m_install.IsRunning())
		m_install.BeginOpen("Choose a mod", kZipFilter);

	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Open the folder")))
		Open(ModPacks::Root());

	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Rescan")))
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

	DrawUserRow();

	for (int i = 0; i < ModPacks::Count(); ++i)
		DrawRow(i);

	ImGui::EndTable();

	if (m_moved >= 0)
		m_dirty = ModPacks::Move(m_moved, m_delta) || m_dirty;

	if (!m_dirty)
		return;

	m_dirty = false;
	m_changed = true;
	ModFiles::Rescan();
}

void ModsPanel::DrawUserRow()
{
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::TextDisabled("--");

	ImGui::TableNextColumn();
	ImGui::TextColored(kUserText, Tr("Your files"));

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", ModFiles::Root());

	ImGui::SameLine();
	UiText::Muted("always on, always first");

	ImGui::TableNextColumn();
	ImGui::Text("%d", ModFiles::UserFileCount());

	ImGui::TableNextColumn();

	if (ImGui::SmallButton(TrLabel("Open")))
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
		UiText::Warn("%d file(s) here are overridden by %s, which is higher in the list.",
			pack->beaten, pack->beatenBy.c_str());
	}

	DrawStages(index, *pack);
	DrawMusic(*pack);

	ImGui::TableNextColumn();
	ImGui::Text("%d", pack->files);

	ImGui::TableNextColumn();
	ImGui::BeginDisabled(index == 0);

	if (ImGui::SmallButton(TrLabel("Up")))
	{
		m_moved = index;
		m_delta = -1;
	}

	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(index + 1 >= ModPacks::Count());

	if (ImGui::SmallButton(TrLabel("Down")))
	{
		m_moved = index;
		m_delta = 1;
	}

	ImGui::EndDisabled();
	ImGui::PopID();
}

void ModsPanel::DrawStages(int index, const ModPacks::Pack& pack)
{
	char ready[kStageListBytes] = {};
	size_t used = 0;

	for (const PackStages::Folder& folder : pack.stages)
	{
		if (!folder.ready || !IsNewStage(folder.number) || used >= sizeof(ready))
			continue;

		const int written = snprintf(ready + used, sizeof(ready) - used, used == 0 ? "%d" : ", %d", folder.number);
		used += written > 0 ? static_cast<size_t>(written) : sizeof(ready);
	}

	if (used != 0)
		UiText::Muted("Stages %s join the stage list when the game starts.", ready);

	for (const PackStages::Folder& folder : pack.stages)
	{
		if (!IsNewStage(folder.number))
			continue;

		ImGui::PushID(folder.number);

		if (folder.ready)
			DrawStageClash(index, folder.number);
		else
			DrawStageInstall(pack, folder.number);

		ImGui::PopID();
	}
}

void ModsPanel::DrawStageClash(int index, int number)
{
	const int owner = ModPacks::StageOwner(number, index);
	const ModPacks::Pack* const other = owner < 0 ? nullptr : ModPacks::At(owner);

	if (other == nullptr)
		return;

	UiText::Warn("Stage %d is also in %s. The mod higher in the list is used.", number, other->name.c_str());
}

void ModsPanel::DrawStageInstall(const ModPacks::Pack& pack, int number)
{
	UiText::Muted("bg%03d is not ready to play yet.", number);
	ImGui::SameLine();
	ImGui::BeginDisabled(StageImport::IsBusy());

	if (ImGui::SmallButton(TrLabel("Install as a stage")))
	{
		const std::string folder = pack.path + "\\bg\\" + PackStages::FolderName(number);
		const bool started = StageImport::InstallFolder(folder.c_str(), pack.name.c_str());

		sprintf_s(m_status, "%s", started
			? "installing. Restart the game when it finishes."
			: StageImport::StatusText());
	}

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(Tr("Builds the stage from its model and adds it to your stages."));
}

void ModsPanel::DrawMusic(const ModPacks::Pack& pack)
{
	BgmOverride::PackMusic music = {};

	if (!BgmOverride::Report(pack.path, music))
		return;

	if (music.tracks > 0)
		UiText::Muted("%d music track(s) join the game's music list.", music.tracks);

	if (music.moved > 0)
		UiText::Muted("%d of them use another slot, because a mod higher in the list uses theirs.", music.moved);

	if (music.dropped > 0)
		UiText::Warn("%d music track(s) are left out: every music slot is in use.", music.dropped);
}

void ModsPanel::DrawFooter()
{
	if (ModPacks::Count() == 0)
	{
		UiText::Muted("No mods installed. Install a zip, or drop a mod's folder into %s.",
			ModPacks::Root().c_str());
		return;
	}

	UiText::Good("%d file(s) from %d mod(s) are in use.", ModPacks::FileCount(),
		ModPacks::EnabledCount());

	if (!m_changed)
		return;

	RestartPrompt::Draw("The music, the stage list and some sound effects are only read when the "
		"game starts. Restart to use every file of the mods you changed.");
}

void ModsPanel::DrawHelp()
{
	ImGui::TextWrapped(Tr("A mod is a folder that uses UNI2's file paths. For example, a mod "
		"that replaces Hyde's voice only holds se\\battle_se\\chr000. Install a zip or drop the "
		"folder in yourself. It shows up in the list within a second, with no restart."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Turning a mod off"));

	ImGui::TextWrapped(Tr("The game uses its original file again the next time it loads it, on the next "
		"match or screen. Nothing is copied over the game and the d archive is never touched, so "
		"removing every mod leaves your install exactly as Steam put it."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("When two mods change the same file"));

	ImGui::TextWrapped(Tr("The mod higher in the list wins that file only. The rest of the lower mod "
		"still applies. The row tells you when this happens and names the winning mod, so use Up "
		"and Down to choose which one wins."));

	ImGui::Spacing();
	ImGui::TextWrapped(Tr("Files are looked up in this order:"));
	ImGui::BulletText(Tr("a loaded game patch"));
	ImGui::BulletText(Tr("Your files (the top row, the Mods folder)"));
	ImGui::BulletText(Tr("this list, top to bottom"));
	ImGui::BulletText(Tr("UNI2's d archive, for everything else"));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Stages are different"));

	ImGui::TextWrapped(Tr("The game reads its stage list only when it starts, so a stage from a mod "
		"appears after a restart. Every bg\\bgNNN folder with a bg.fbx.bin is added, with the values "
		"from its stage.txt. If two mods use the same number, the higher one is used."));

	ImGui::Spacing();
	ImGui::TextWrapped(Tr("A folder with only a model to convert gets an Install as a stage button "
		"instead."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Music"));

	ImGui::TextWrapped(Tr("Every mod's Bgm\\bgm.txt is added to the game's music list, with yours, after "
		"a restart. If two mods use the same slot, the higher one keeps it and the other track moves "
		"to a free slot, so both can be played. The mod's grpdat\\CSel\\bgmselect.txt follows the move."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Making a mod"));

	ImGui::TextWrapped(Tr("Put your files at the same paths the game uses, and add a mod.ini to give "
		"it a name. A folder without one still works and is listed by its folder name."));

	ImGui::Spacing();
	ImGui::TextUnformatted(Tr("[Mod]\nName = Hyde speaks UNI\nAuthor = you\nVersion = 1.0\n"
		"Note = His UNI[st] voice, all 96 lines."));

	ImGui::Spacing();
	ImGui::TextWrapped(Tr("A stage can have a section, named after its folder, to set its name "
		"and its picture in your mod's stage select sheet:"));

	ImGui::Spacing();
	ImGui::TextUnformatted(Tr("[bg068]\nName = Garden at Night\nCard = 30"));

	ImGui::Spacing();
	ImGui::TextWrapped(Tr("To share a mod, send the folder. Your switches and order stay in your "
		"ini."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Online"));

	ImGui::TextWrapped(Tr("Mods stay loaded when you play online. Art and sound only change what you "
		"see, but a mod with data or script files changes gameplay and will desync with the other "
		"player."));
}
