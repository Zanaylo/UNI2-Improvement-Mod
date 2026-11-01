#include "Overlay/Panels/ModsPanel.h"

#include "Core/ShellOpen.h"
#include "Core/Text/Language.h"
#include "Core/Text/TextList.h"
#include "Game/Files/ModFiles.h"
#include "Game/Files/ModPacks.h"
#include "Overlay/Framework/FileDrop.h"
#include "Overlay/Panels/RestartPrompt.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

constexpr float kListShare = 0.5f;
constexpr float kMinListHeight = 160.0f;
constexpr float kSwitchColumn = 28.0f;
constexpr float kFilesColumn = 56.0f;
constexpr float kSearchWidth = 200.0f;
constexpr float kFooterLines = 4.0f;
constexpr const char* kZipFilter = "A mod (*.zip)\0*.zip\0All files\0*.*\0";
constexpr const char* kRowPayload = "UNI2_MOD_ROW";
constexpr int kNoIndex = -1;

const ImVec4 kUserText(0.45f, 0.90f, 0.50f, 1.0f);
const ImVec4 kGameplayColour(0.95f, 0.70f, 0.35f, 1.0f);
const ImVec4 kProblemColour(0.95f, 0.55f, 0.45f, 1.0f);

void HoverTip(const char* text)
{
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", Tr(text));
}

}

void ModsPanel::Draw()
{
	FileDrop::Accept(true);
	TakeInstalls();

	if (!ImGui::BeginTabBar("##modstabs"))
		return;

	if (ImGui::BeginTabItem(TrLabel("Mods")))
	{
		DrawTools();
		DrawListTools();

		const ImVec2 room = ImGui::GetContentRegionAvail();
		const float height = (std::max)(room.y - ImGui::GetFrameHeightWithSpacing() * kFooterLines,
			Ui::Scaled(kMinListHeight));

		DrawList(ImVec2(room.x * kListShare, height));
		ImGui::SameLine();

		if (ImGui::BeginChild("##moddetailpane", ImVec2(0.0f, height), ImGuiChildFlags_Borders))
			DrawDetails();

		ImGui::EndChild();

		ApplyMove();
		DrawFooter();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem(TrLabel("Help")))
	{
		DrawHelp();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();

	if (!m_dirty)
		return;

	m_dirty = false;
	ModFiles::Rescan();
}

void ModsPanel::OnClosed()
{
	FileDrop::Accept(false);
	m_details.Release();
}

void ModsPanel::TakeInstalls()
{
	std::string picked;

	if (m_zipDialog.TakeResult(picked) && !picked.empty())
		Install(picked);

	if (m_folderDialog.TakeResult(picked) && !picked.empty())
		Install(picked);

	std::vector<std::string> dropped;

	if (FileDrop::Take(dropped))
	{
		for (const std::string& path : dropped)
			Install(path);
	}

	ProcessQueue();
}

void ModsPanel::Install(const std::string& source)
{
	m_queue.push_back(source);
	m_changed = true;
}

void ModsPanel::ProcessQueue()
{
	if (m_queue.empty() || ModPacks::IsInstalling())
		return;

	ModPacks::BeginInstall(m_queue.front());
	m_queue.erase(m_queue.begin());
}

void ModsPanel::DrawTools()
{
	if (ImGui::Button(TrLabel("Install a zip...")) && !m_zipDialog.IsRunning())
		m_zipDialog.BeginOpen(Tr("Choose a mod"), kZipFilter);

	HoverTip("You can also drop a zip or a folder on the game window.");
	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Install a folder...")) && !m_folderDialog.IsRunning())
		m_folderDialog.BeginFolder(Tr("Choose a mod's folder"));

	HoverTip("Copies the folder into your mods. The original stays where it is.");
	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Open the folder")))
		ShellOpen::Open(ModPacks::Root());

	ImGui::SameLine();
	ImGui::BeginDisabled(ModPacks::IsScanning());

	if (ImGui::Button(TrLabel("Rescan")))
		ModPacks::RequestScan();

	ImGui::EndDisabled();
	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Sort by rules")))
	{
		if (ModPacks::SortByRules())
			MarkChanged();
		else
			snprintf(m_status, sizeof(m_status), "%s", Tr("The mods' rules contradict each other, so nothing was moved."));
	}

	HoverTip("Moves each mod to where its mod.ini asks to be.");

	if (m_profiles.Draw())
		MarkChanged();

	if (ModPacks::IsInstalling() || !m_queue.empty())
		UiText::Muted("%s", ModPacks::InstallStatus().c_str());
	else if (ModPacks::IsScanning())
		UiText::Muted("Looking for changes in the mods folder...");
	else if (m_status[0] != '\0')
		UiText::Muted("%s", m_status);
}

void ModsPanel::DrawListTools()
{
	Ui::SetItemWidth(kSearchWidth);

	if (ImGui::InputTextWithHint("##modsearch", Tr("Search"), m_search.InputBuf, IM_ARRAYSIZE(m_search.InputBuf)))
		m_search.Build();

	ImGui::SameLine();

	if (ImGui::Button(TrLabel("All on")))
	{
		ModPacks::SetAllEnabled(true);
		MarkChanged();
	}

	ImGui::SameLine();

	if (ImGui::Button(TrLabel("All off")))
	{
		ModPacks::SetAllEnabled(false);
		MarkChanged();
	}
}

void ModsPanel::DrawList(const ImVec2& size)
{
	if (!ImGui::BeginTable("##mods", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY, size))
	{
		return;
	}

	ImGui::TableSetupColumn(Tr("On"), ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kSwitchColumn));
	ImGui::TableSetupColumn(Tr("Reading order"));
	ImGui::TableSetupColumn(Tr("Files"), ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kFilesColumn));
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	DrawUserRow();

	for (int i = 0; i < ModPacks::Count(); ++i)
		DrawRow(i);

	ImGui::EndTable();
}

void ModsPanel::DrawUserRow()
{
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::TextDisabled("--");

	ImGui::TableNextColumn();
	ImGui::TextColored(kUserText, "%s", Tr("Your files"));

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", ModFiles::Root());

	ImGui::SameLine();

	if (ImGui::SmallButton(TrLabel("Open##userfiles")))
		ShellOpen::Open(ModFiles::Root());

	UiText::Muted("always on, always first");

	ImGui::TableNextColumn();
	ImGui::Text("%d", ModFiles::UserFileCount());
}

void ModsPanel::DrawRow(int index)
{
	const ModPacks::Pack* const pack = ModPacks::At(index);

	if (pack == nullptr || !Matches(*pack))
		return;

	ImGui::PushID(index);
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	bool enabled = pack->enabled;

	if (ImGui::Checkbox("##on", &enabled))
	{
		const int extra = ModPacks::SetEnabled(index, enabled);

		if (extra > 0)
			snprintf(m_status, sizeof(m_status), "%s", TrFormat("%d mod(s) it needs were turned on too.", extra).c_str());

		MarkChanged();
	}

	ImGui::TableNextColumn();

	if (!pack->enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);

	if (ImGui::Selectable(pack->manifest.name.c_str(), TextList::SameNoCase(m_selected, pack->id),
		ImGuiSelectableFlags_AllowOverlap))
	{
		m_selected = pack->id;
	}

	if (!pack->enabled)
		ImGui::PopStyleColor();

	DrawDragAndDrop(index, *pack);
	DrawRowMenu(index);
	DrawRowBadges(*pack);

	ImGui::TableNextColumn();
	ImGui::Text("%d", pack->files);

	ImGui::PopID();
}

void ModsPanel::DrawRowBadges(const ModPacks::Pack& pack)
{
	if (pack.gameplayFiles > 0)
	{
		ImGui::SameLine();
		ImGui::TextColored(kGameplayColour, "%s", Tr("gameplay"));
		HoverTip("Changes gameplay. Online, the other player needs it too.");
	}

	if (!pack.issues.empty() || !pack.wrapper.empty())
	{
		ImGui::SameLine();
		ImGui::TextColored(kProblemColour, "(!)");
		HoverTip("Needs attention. Select it to see why.");
	}

	if (pack.beaten > 0)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", Tr("partly replaced"));

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", TrFormat("%d file(s) are used from %s instead.", pack.beaten,
				pack.beatenBy.c_str()).c_str());
		}
	}
}

void ModsPanel::DrawRowMenu(int index)
{
	if (!ImGui::BeginPopupContextItem("##rowmenu"))
		return;

	const int last = ModPacks::Count() - 1;
	int to = kNoIndex;

	if (ImGui::MenuItem(TrLabel("Move to the top"), nullptr, false, index > 0))
		to = 0;

	if (ImGui::MenuItem(TrLabel("Move up"), nullptr, false, index > 0))
		to = index - 1;

	if (ImGui::MenuItem(TrLabel("Move down"), nullptr, false, index < last))
		to = index + 1;

	if (ImGui::MenuItem(TrLabel("Move to the bottom"), nullptr, false, index < last))
		to = last;

	if (to != kNoIndex)
	{
		m_moveFrom = index;
		m_moveTo = to;
	}

	ImGui::Separator();

	if (ImGui::MenuItem(TrLabel("Only this mod on")))
	{
		ModPacks::SetAllEnabled(false);
		ModPacks::SetEnabled(index, true);
		MarkChanged();
	}

	const ModPacks::Pack* const pack = ModPacks::At(index);

	if (ImGui::MenuItem(TrLabel("Open its folder")) && pack != nullptr)
		ShellOpen::Open(pack->path);

	ImGui::EndPopup();
}

void ModsPanel::DrawDragAndDrop(int index, const ModPacks::Pack& pack)
{
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload(kRowPayload, &index, sizeof(index));
		ImGui::TextUnformatted(pack.manifest.name.c_str());
		ImGui::EndDragDropSource();
	}

	if (!ImGui::BeginDragDropTarget())
		return;

	if (const ImGuiPayload* const payload = ImGui::AcceptDragDropPayload(kRowPayload))
	{
		m_moveFrom = *static_cast<const int*>(payload->Data);
		m_moveTo = index;
	}

	ImGui::EndDragDropTarget();
}

void ModsPanel::ApplyMove()
{
	if (m_moveFrom != kNoIndex && m_moveTo != kNoIndex && ModPacks::MoveTo(m_moveFrom, m_moveTo))
		MarkChanged();

	m_moveFrom = kNoIndex;
	m_moveTo = kNoIndex;
}

void ModsPanel::DrawDetails()
{
	if (m_details.Draw(SelectedIndex(), m_status, sizeof(m_status)))
		MarkChanged();
}

void ModsPanel::DrawFooter()
{
	if (ModPacks::Count() == 0)
	{
		UiText::Muted("No mods installed. Install a zip, or drop a mod's folder into %s.",
			ModPacks::Root().c_str());
		return;
	}

	UiText::Good("%d file(s) from %d mod(s) are in use.", ModPacks::FileCount(), ModPacks::EnabledCount());

	if (ModPacks::IssueCount() > 0)
		UiText::Warn("%d problem(s) need attention. The mods marked (!) say why.", ModPacks::IssueCount());

	if (!m_changed)
		return;

	RestartPrompt::Draw("The music, the stage list and some sound effects are only read when the "
		"game starts. Restart to use every file of the mods you changed.");
}

void ModsPanel::DrawHelp()
{
	ImGui::TextWrapped("%s", Tr("A mod is a folder that uses UNI2's file paths. For example, a mod "
		"that replaces Hyde's voice only holds se\\battle_se\\chr000. Install a zip or drop the "
		"folder in yourself. It shows up in the list within a second, with no restart."));

	ImGui::Spacing();
	ImGui::TextWrapped("%s", Tr("While this window is open you can also drop zips and folders on the game "
		"window to install them."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Turning a mod off"));

	ImGui::TextWrapped("%s", Tr("The game uses its original file again the next time it loads it, on the next "
		"match or screen. Nothing is copied over the game and the d archive is never touched, so "
		"removing every mod leaves your install exactly as Steam put it."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("When two mods change the same file"));

	ImGui::TextWrapped("%s", Tr("The mod higher in the list wins that file only. The rest of the lower mod "
		"still applies. Drag a mod up or down, or right-click it, to choose which one wins. The Conflicts "
		"tab of a mod lists every shared file and which mod wins it."));

	ImGui::Spacing();
	ImGui::TextWrapped("%s", Tr("Files are looked up in this order:"));
	ImGui::BulletText("%s", Tr("a loaded game patch"));
	ImGui::BulletText("%s", Tr("Your files (the top row, the Mods folder)"));
	ImGui::BulletText("%s", Tr("this list, top to bottom"));
	ImGui::BulletText("%s", Tr("UNI2's d archive, for everything else"));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Profiles"));

	ImGui::TextWrapped("%s", Tr("A profile saves which mods are on and their order. Pick one to switch the "
		"whole list at once. A * after its name means the list changed since it was saved."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Stages are different"));

	ImGui::TextWrapped("%s", Tr("The game reads its stage list only when it starts, so a stage from a mod "
		"appears after a restart. Every bg\\bgNNN folder with a bg.fbx.bin is added, with the values "
		"from its stage.txt. If two mods use the same number, the higher one is used."));

	ImGui::Spacing();
	ImGui::TextWrapped("%s", Tr("A folder with only a model to convert gets an Install as a stage button "
		"instead."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Music"));

	ImGui::TextWrapped("%s", Tr("Every mod's Bgm\\bgm.txt is added to the game's music list, with yours, after "
		"a restart. If two mods use the same slot, the higher one keeps it and the other track moves "
		"to a free slot, so both can be played. The mod's grpdat\\CSel\\bgmselect.txt follows the move."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Making a mod"));

	ImGui::TextWrapped("%s", Tr("Put your files at the same paths the game uses, and add a mod.ini to give "
		"it a name. A folder without one still works and is listed by its folder name."));

	ImGui::Spacing();
	ImGui::TextUnformatted(Tr("[Mod]\nName = Hyde speaks UNI\nAuthor = you\nVersion = 1.0\n"
		"Note = His UNI[st] voice, all 96 lines."));

	ImGui::Spacing();
	ImGui::TextWrapped("%s", Tr("Optional keys in [Mod]:"));
	ImGui::BulletText("%s", Tr("Url: the mod's web page"));
	ImGui::BulletText("%s", Tr("Category: one word to group it, like Voices"));
	ImGui::BulletText("%s", Tr("Requires: mods it needs, by folder name or Id"));
	ImGui::BulletText("%s", Tr("Conflicts: mods it does not work with"));
	ImGui::BulletText("%s", Tr("Above and Below: mods it must sit above or below"));
	ImGui::BulletText("%s", Tr("Id: the name other mods use for it. Default: the folder name"));

	ImGui::Spacing();
	ImGui::TextWrapped("%s", Tr("A stage can have a section, named after its folder, to set its name "
		"and its picture in your mod's stage select sheet:"));

	ImGui::Spacing();
	ImGui::TextUnformatted(Tr("[bg068]\nName = Garden at Night\nCard = 30"));

	ImGui::Spacing();
	ImGui::TextWrapped("%s", Tr("To share a mod, send the folder. Your switches and order stay in your "
		"ini."));

	ImGui::Spacing();
	ImGui::SeparatorText(Tr("Online"));

	ImGui::TextWrapped("%s", Tr("Mods stay loaded when you play online. Art and sound only change what you "
		"see, but a mod with data or script files changes gameplay and will desync with the other "
		"player."));

	ImGui::Spacing();
	ImGui::TextWrapped("%s", Tr("Mods with such files are marked gameplay in the list."));
}

bool ModsPanel::Matches(const ModPacks::Pack& pack) const
{
	if (!m_search.IsActive())
		return true;

	const ModManifest& manifest = pack.manifest;

	return m_search.PassFilter(manifest.name.c_str()) || m_search.PassFilter(manifest.author.c_str()) ||
		m_search.PassFilter(manifest.category.c_str()) || m_search.PassFilter(pack.id.c_str());
}

int ModsPanel::SelectedIndex() const
{
	for (int i = 0; i < ModPacks::Count(); ++i)
	{
		if (TextList::SameNoCase(ModPacks::At(i)->id, m_selected))
			return i;
	}

	return kNoIndex;
}

void ModsPanel::MarkChanged()
{
	m_dirty = true;
	m_changed = true;
}
