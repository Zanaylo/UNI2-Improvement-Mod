#include "Overlay/Panels/ModDetails.h"

#include "Core/ShellOpen.h"
#include "Core/Text/Language.h"
#include "Core/Text/TextList.h"
#include "Game/Audio/BgmOverride.h"
#include "Game/Files/ModLayout.h"
#include "Game/Stages/StageImport.h"
#include "Game/Stages/StageLibrary.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <cstdio>

namespace {

constexpr float kClashListHeight = 160.0f;
constexpr size_t kStageListBytes = 256;
constexpr const char* kListJoint = ", ";

const ImVec4 kGameplayColour(0.95f, 0.70f, 0.35f, 1.0f);

void WarnIssue(const ModRules::Issue& issue)
{
	const char* const other = issue.other.c_str();

	switch (issue.kind)
	{
	case ModRules::IssueKind::MissingRequirement:
		UiText::Warn("Needs %s, which is not installed.", other);
		return;
	case ModRules::IssueKind::DisabledRequirement:
		UiText::Warn("Needs %s, which is off.", other);
		return;
	case ModRules::IssueKind::Conflict:
		UiText::Warn("Does not work with %s. Turn one of them off.", other);
		return;
	case ModRules::IssueKind::ShouldBeAbove:
		UiText::Warn("Should be above %s in the list. Sort by rules fixes it.", other);
		return;
	case ModRules::IssueKind::ShouldBeBelow:
		UiText::Warn("Should be below %s in the list. Sort by rules fixes it.", other);
		return;
	}
}

bool IsNewStage(int number)
{
	return number <= StageLibrary::kSlotLast && !StageLibrary::IsGameStage(number);
}

std::string Joined(const std::vector<std::string>& items)
{
	std::string joined;

	for (const std::string& item : items)
		joined += (joined.empty() ? "" : kListJoint) + item;

	return joined;
}

void SplitClashes(const std::vector<ModPacks::Clash>& clashes, std::vector<ModPacks::Clash>& won,
	std::vector<ModPacks::Clash>& lost)
{
	won.clear();
	lost.clear();

	for (const ModPacks::Clash& clash : clashes)
		(clash.wins ? won : lost).push_back(clash);
}

}

bool ModDetails::Draw(int index, char* status, int statusSize)
{
	const ModPacks::Pack* const pack = ModPacks::At(index);

	if (pack == nullptr)
	{
		UiText::Muted("Pick a mod in the list to see its details.");
		return false;
	}

	Refresh(index);

	if (!ImGui::BeginTabBar("##moddetails"))
		return false;

	bool changed = false;

	if (ImGui::BeginTabItem(TrLabel("About")))
	{
		DrawAbout(*pack);
		changed = DrawProblems(index, *pack, status, statusSize);
		DrawStages(index, *pack, status, statusSize);
		DrawMusic(*pack);
		changed = DrawRemove(index, *pack, status, statusSize) || changed;
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem(TrLabel("Files")))
	{
		DrawFiles();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem(TrLabel("Conflicts")))
	{
		DrawConflicts();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
	return changed;
}

void ModDetails::Release()
{
	std::vector<std::string>().swap(m_files);
	std::vector<ModPacks::Clash>().swap(m_won);
	std::vector<ModPacks::Clash>().swap(m_lost);
	std::vector<int>().swap(m_shown);
	m_revision = -1;
	m_index = -1;
}

void ModDetails::Refresh(int index)
{
	if (m_revision == ModPacks::Revision() && m_index == index)
		return;

	m_revision = ModPacks::Revision();
	m_index = index;
	m_files = ModPacks::FilesOf(index);
	SplitClashes(ModPacks::ClashesOf(index), m_won, m_lost);
	m_filesDirty = true;
}

void ModDetails::DrawAbout(const ModPacks::Pack& pack)
{
	const ModManifest& manifest = pack.manifest;

	ImGui::TextUnformatted(manifest.name.c_str());

	if (!manifest.version.empty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", manifest.version.c_str());
	}

	if (!manifest.author.empty())
		UiText::Muted("by %s", manifest.author.c_str());

	if (!manifest.category.empty())
		UiText::Muted("Category: %s", manifest.category.c_str());

	if (!manifest.description.empty())
		ImGui::TextWrapped("%s", manifest.description.c_str());

	if (!manifest.described)
		UiText::Muted("No mod.ini, so it is listed by its folder name.");

	UiText::Muted("%d file(s)", pack.files);

	if (!manifest.requirements.empty())
		UiText::Muted("Needs: %s", Joined(manifest.requirements).c_str());

	if (pack.gameplayFiles > 0)
	{
		UiText::Warn("%d file(s) change gameplay. Online, the other player needs this mod too, or the match "
			"will desync.", pack.gameplayFiles);
	}

	if (manifest.HasWebLink())
	{
		if (ImGui::SmallButton(TrLabel("Open the mod's page")))
			ShellOpen::Open(manifest.url);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", manifest.url.c_str());

		ImGui::SameLine();
	}

	if (ImGui::SmallButton(TrLabel("Open its folder")))
		ShellOpen::Open(pack.path);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", pack.path.c_str());
}

bool ModDetails::DrawProblems(int index, const ModPacks::Pack& pack, char* status, int statusSize)
{
	bool changed = false;

	if (!pack.wrapper.empty())
	{
		UiText::Warn("The files are inside an extra folder, %s, so the game cannot find them.", pack.wrapper.c_str());

		if (ImGui::SmallButton(TrLabel("Fix the folder")))
			changed = ModPacks::Unwrap(index, status, statusSize);
	}

	if (pack.beaten > 0)
	{
		UiText::Warn("%d file(s) here are overridden by %s, which is higher in the list.", pack.beaten,
			pack.beatenBy.c_str());
	}

	const std::vector<ModRules::Issue> issues = pack.issues;

	for (size_t i = 0; i < issues.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		changed = DrawIssue(issues[i]) || changed;
		ImGui::PopID();
	}

	return changed;
}

bool ModDetails::DrawIssue(const ModRules::Issue& issue)
{
	WarnIssue(issue);

	if (issue.kind != ModRules::IssueKind::DisabledRequirement)
		return false;

	if (!ImGui::SmallButton(TrLabel("Turn it on")))
		return false;

	ModPacks::SetEnabled(issue.otherIndex, true);
	return true;
}

void ModDetails::DrawStages(int index, const ModPacks::Pack& pack, char* status, int statusSize)
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

		const int owner = folder.ready ? ModPacks::StageOwner(folder.number, index) : -1;
		const ModPacks::Pack* const other = ModPacks::At(owner);

		if (other != nullptr)
		{
			UiText::Warn("Stage %d is also in %s. The mod higher in the list is used.", folder.number,
				other->manifest.name.c_str());
		}

		if (!folder.ready)
		{
			UiText::Muted("bg%03d is not ready to play yet.", folder.number);
			ImGui::BeginDisabled(StageImport::IsBusy());

			if (ImGui::SmallButton(TrLabel("Install as a stage")))
			{
				const std::string path = pack.path + "\\bg\\" + PackStages::FolderName(folder.number);
				const bool started = StageImport::InstallFolder(path.c_str(), pack.manifest.name.c_str());

				snprintf(status, statusSize, "%s", started
					? Tr("installing. Restart the game when it finishes.")
					: StageImport::StatusText());
			}

			ImGui::EndDisabled();

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("%s", Tr("Builds the stage from its model and adds it to your stages."));
		}

		ImGui::PopID();
	}
}

void ModDetails::DrawMusic(const ModPacks::Pack& pack)
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

bool ModDetails::DrawRemove(int index, const ModPacks::Pack& pack, char* status, int statusSize)
{
	ImGui::Spacing();
	ImGui::Separator();

	if (ImGui::SmallButton(TrLabel("Move to the Recycle Bin...")))
		ImGui::OpenPopup("##removemod");

	if (!ImGui::BeginPopup("##removemod"))
		return false;

	ImGui::TextUnformatted(TrFormat("Move %s to the Recycle Bin?", pack.manifest.name.c_str()).c_str());

	bool removed = false;

	if (ImGui::Button(TrLabel("Yes, remove it")))
	{
		removed = ModPacks::Uninstall(index, status, statusSize);
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();

	if (ImGui::Button(TrLabel("Cancel")))
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();
	return removed;
}

void ModDetails::DrawFiles()
{
	Ui::SetItemWidth(-1.0f);

	if (ImGui::InputTextWithHint("##modfilefilter", Tr("Filter the files"), m_filter.InputBuf,
		IM_ARRAYSIZE(m_filter.InputBuf)))
	{
		m_filter.Build();
		m_filesDirty = true;
	}

	if (m_filesDirty)
	{
		m_shown.clear();

		for (int i = 0; i < static_cast<int>(m_files.size()); ++i)
		{
			if (m_filter.PassFilter(m_files[i].c_str()))
				m_shown.push_back(i);
		}

		m_filesDirty = false;
	}

	UiText::Muted("%d of %d file(s). Orange files change gameplay.", static_cast<int>(m_shown.size()),
		static_cast<int>(m_files.size()));

	if (!ImGui::BeginChild("##modfiles", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
	{
		ImGui::EndChild();
		return;
	}

	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(m_shown.size()));

	while (clipper.Step())
	{
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
		{
			const std::string& key = m_files[m_shown[row]];

			if (ModLayout::ChangesGameplay(key))
				ImGui::TextColored(kGameplayColour, "%s", key.c_str());
			else
				ImGui::TextUnformatted(key.c_str());
		}
	}

	ImGui::EndChild();
}

void ModDetails::DrawConflicts()
{
	if (m_won.empty() && m_lost.empty())
	{
		UiText::Muted("No file of this mod is in another mod that is on.");
		return;
	}

	ImGui::SeparatorText(TrFormat("Replaced by a mod higher in the list (%d)", static_cast<int>(m_lost.size())).c_str());
	DrawClashList("##lost", m_lost);

	ImGui::SeparatorText(TrFormat("Used from this mod (%d)", static_cast<int>(m_won.size())).c_str());
	DrawClashList("##won", m_won);
}

void ModDetails::DrawClashList(const char* id, const std::vector<ModPacks::Clash>& clashes)
{
	if (clashes.empty())
	{
		UiText::Muted("None.");
		return;
	}

	if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter,
		ImVec2(0.0f, Ui::Scaled(kClashListHeight))))
	{
		return;
	}

	ImGui::TableSetupColumn(Tr("File"));
	ImGui::TableSetupColumn(Tr("Other mod"));
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(clashes.size()));

	while (clipper.Step())
	{
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(clashes[row].key.c_str());
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", clashes[row].other.c_str());
		}
	}

	ImGui::EndTable();
}
