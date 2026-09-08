#include "Overlay/Window/StagesPanel.h"

#include "Game/BgGrade.h"
#include "Game/ExtraStages.h"
#include "Game/GameRestart.h"
#include "Game/BgCeiling.h"
#include "Game/StageImport.h"
#include "Game/StageLibrary.h"
#include "Game/StageObjects.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr float kListHeight = 220.0f;
constexpr float kNumberColumn = 60.0f;
constexpr float kSizeColumn = 80.0f;
constexpr float kActionColumn = 150.0f;
constexpr float kCheckboxGap = 24.0f;
constexpr float kPortedHeight = 300.0f;
constexpr float kGradeColumn = 110.0f;
constexpr float kInGameColumn = 70.0f;

const ImVec4 kPlayingText(0.45f, 0.90f, 0.50f, 1.0f);

void Megabytes(uint32_t bytes, char* out, size_t size)
{
	sprintf_s(out, size, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

constexpr int kUnbound = 10000;

struct Listed
{
	int slot;
	int id;
	int key;
	bool shown;
	std::string name;
	std::string from;
	bool yours;
};

int Order(const Listed& row)
{
	return row.slot >= 0 ? row.slot : kUnbound + row.id;
}

void Gather(std::vector<Listed>& out)
{
	out.clear();

	for (int i = 0; i < ExtraStages::StageCount(); ++i)
	{
		const ExtraStages::Stage* const stage = ExtraStages::StageAt(i);

		if (stage == nullptr || !StageLibrary::GameOwns(stage->number))
			continue;

		Listed row = {};
		row.slot = stage->number;
		row.id = -1;
		row.key = stage->number;
		row.shown = true;
		row.name = stage->name.empty() ? stage->folder : stage->name;
		row.from = "the game's own, bg\\" + stage->folder;
		row.yours = false;

		out.push_back(row);
	}

	std::vector<StageLibrary::Entry> library;
	StageLibrary::Snapshot(library);

	for (const StageLibrary::Entry& entry : library)
	{
		if (StageImport::Dropped(entry.id))
			continue;

		char folder[24] = {};
		sprintf_s(folder, ", bg%03d", entry.id);

		Listed row = {};
		row.slot = entry.slot;
		row.id = entry.id;
		row.key = entry.id;
		row.shown = entry.shown;
		row.name = entry.name;
		row.from = entry.game + " " + entry.folder + folder;
		row.yours = true;

		out.push_back(row);
	}

	std::sort(out.begin(), out.end(),
		[](const Listed& a, const Listed& b) { return Order(a) < Order(b); });
}

}

void StagesPanel::SyncRows()
{
	const std::string scanned = StageImport::ScannedGame() == nullptr
		? std::string() : StageImport::ScannedGame();
	const int count = StageImport::OfferCount();

	if (scanned == m_rowsFor && count == m_rowCount)
		return;

	m_rowsFor = scanned;
	m_rowCount = count;
	m_queue.clear();
	m_rows.assign(static_cast<size_t>(count < 0 ? 0 : count), Row());

	for (int i = 0; i < count; ++i)
	{
		const StageImport::Offer* const offer = StageImport::OfferAt(i);

		if (offer == nullptr)
			continue;

		strncpy_s(m_rows[i].name, offer->name.empty() ? offer->folder.c_str()
			: offer->name.c_str(), _TRUNCATE);
	}
}

void StagesPanel::Queue(int index)
{
	for (int waiting : m_queue)
	{
		if (waiting == index)
			return;
	}

	m_queue.push_back(index);
}

void StagesPanel::PumpQueue()
{
	if (m_queue.empty() || StageImport::IsBusy())
		return;

	std::vector<int> indices;
	std::vector<const char*> names;

	for (int index : m_queue)
	{
		if (index < 0 || index >= static_cast<int>(m_rows.size()))
			continue;

		indices.push_back(index);
		names.push_back(m_rows[index].name);
	}

	m_queue.clear();

	if (indices.empty())
		return;

	StageImport::InstallMany(indices.data(), names.data(), static_cast<int>(indices.size()));
}

void StagesPanel::Draw()
{
	SyncRows();
	PumpQueue();

	if (ImGui::BeginTabBar("##stagetabs"))
	{
		if (ImGui::BeginTabItem("Installed Stages"))
		{
			DrawHidden();

			ImGui::Separator();
			DrawPorted();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Add stages"))
		{
			DrawSource();

			ImGui::Separator();
			DrawCustom();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Help"))
		{
			DrawHelp();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	DrawRestart();
}

void StagesPanel::DrawHidden()
{
	UiText::Muted("Hidden stages");

	if (!ExtraStages::Ready())
		return;

	if (ExtraStages::Count() == 0)
	{
		ImGui::TextDisabled("This build hides no stage.");
		return;
	}

	for (int i = 0; i < ExtraStages::Count(); ++i)
	{
		const ExtraStages::Stage* const stage = ExtraStages::Get(i);

		if (stage == nullptr)
			continue;

		if (i > 0)
			ImGui::SameLine(0.0f, Ui::Scaled(kCheckboxGap));

		ImGui::PushID(stage->number);

		bool unlocked = stage->unlocked;
		char label[128] = {};

		sprintf_s(label, "%s##%d", stage->name.empty() ? "unnamed" : stage->name.c_str(),
			stage->number);

		if (ImGui::Checkbox(label, &unlocked))
			ExtraStages::SetUnlocked(stage->number, unlocked);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Stage %d, bg\\%s.", stage->number, stage->folder.c_str());

		ImGui::PopID();
	}
}

void StagesPanel::DrawSource()
{
	UiText::Muted("Take a stage from another game you own");

	ImGui::BeginDisabled(StageImport::IsBusy());

	if (ImGui::Button("Get stages from French-Bread games"))
		m_sourceDialog.BeginFolder("Pick the game folder to take a stage from");

	ImGui::EndDisabled();

	std::string picked;

	if (m_sourceDialog.TakeResult(picked))
	{
		m_queue.clear();
		m_rowsFor.clear();
		m_rowCount = -1;
		StageImport::Scan(picked.c_str());
	}

	if (StageImport::IsBusy())
	{
		ImGui::ProgressBar(StageImport::Progress() / 100.0f, ImVec2(Ui::Scaled(240.0f), 0.0f));

		if (m_queue.empty())
			UiText::Warn("%s", StageImport::StatusText());
		else
			UiText::Warn("%s (%d more queued)", StageImport::StatusText(),
				static_cast<int>(m_queue.size()));

		return;
	}

	UiText::Muted("%s", StageImport::StatusText());
	DrawOffers();
}

void StagesPanel::DrawCustom()
{
	UiText::Muted("Install a stage of your own");

	ImGui::BeginDisabled(StageImport::IsBusy());

	if (ImGui::Button("Import a stage folder"))
		m_customDialog.BeginFolder("Pick the folder holding the stage's files");

	ImGui::EndDisabled();

	std::string picked;

	if (m_customDialog.TakeResult(picked))
		StageImport::InstallFolder(picked.c_str(), nullptr);
}

void StagesPanel::DrawOffers()
{
	if (StageImport::OfferCount() == 0)
		return;

	if (ImGui::Button("Add all"))
	{
		for (int i = 0; i < StageImport::OfferCount(); ++i)
			Queue(i);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Installs every stage listed, under the names shown, several at a "
			"time. Any that the picker has no room for land in your library unticked.");
	}

	ImGui::SameLine();
	UiText::Muted("Edit a name before you add it if you want to.");

	if (!ImGui::BeginTable("##stageoffers", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(kListHeight))))
	{
		return;
	}

	ImGui::TableSetupColumn("Folder", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kNumberColumn));
	ImGui::TableSetupColumn("Stage");
	ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kSizeColumn));
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kActionColumn));
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (int i = 0; i < StageImport::OfferCount(); ++i)
	{
		ImGui::PushID(i);
		DrawOfferRow(i);
		ImGui::PopID();
	}

	ImGui::EndTable();
}

void StagesPanel::DrawOfferRow(int index)
{
	const StageImport::Offer* const offer = StageImport::OfferAt(index);

	if (offer == nullptr)
		return;

	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(offer->folder.c_str());

	ImGui::TableNextColumn();

	if (index < static_cast<int>(m_rows.size()))
	{
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##name", m_rows[index].name, sizeof(m_rows[index].name));
	}

	ImGui::TableNextColumn();
	char size[32] = {};
	Megabytes(offer->bytes, size, sizeof(size));
	ImGui::TextUnformatted(size);

	ImGui::TableNextColumn();

	bool queued = false;

	for (int waiting : m_queue)
		queued = queued || waiting == index;

	if (queued)
	{
		ImGui::TextDisabled("queued");
		return;
	}

	if (ImGui::SmallButton("Add"))
		Queue(index);
}

void StagesPanel::DrawRoom()
{
	const int budget = StageLibrary::SlotBudget();

	if (!BgCeiling::Lifted())
		UiText::Warn("Stage table: %s", BgCeiling::StatusText());

	if (!BgGrade::Reached())
		UiText::Warn("Colour: %s", BgGrade::StatusText());

	UiText::Muted("%d/%d stages.", StageLibrary::Total(), BgCeiling::Numbers());

	if (StageLibrary::ShownCount() >= budget)
		UiText::Warn("The picker is full at %d stage(s). Take one out to put another in.", budget);
}

void StagesPanel::DrawPorted()
{
	if (ExtraStages::StageCount() == 0 && StageLibrary::Count() == 0)
	{
		ImGui::BeginChild("##noports", ImVec2(0.0f, Ui::Scaled(kPortedHeight)), false);
		ImGui::TextDisabled("None yet.");
		ImGui::EndChild();
		return;
	}

	DrawRoom();

	if (!ImGui::BeginTable("##stageports", 6,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(kPortedHeight))))
	{
		return;
	}

	ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kNumberColumn));
	ImGui::TableSetupColumn("Name");
	ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed,
		Ui::Scaled(kInGameColumn));
	ImGui::TableSetupColumn("Lift", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kGradeColumn));
	ImGui::TableSetupColumn("Contrast", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kGradeColumn));
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kActionColumn));
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	std::vector<Listed> rows;
	Gather(rows);

	const int playing = ExtraStages::LoadedStage();

	for (const Listed& row : rows)
	{
		ImGui::PushID(row.key);
		ImGui::TableNextRow();

		const bool inMatch = row.slot >= 0 && row.slot == playing;

		ImGui::TableNextColumn();

		if (row.slot >= 0)
			ImGui::Text("%d", row.slot);
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();

		if (inMatch)
			ImGui::TextColored(kPlayingText, "%s", row.name.c_str());
		else
			ImGui::TextUnformatted(row.name.c_str());

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", row.from.c_str());

		ImGui::TableNextColumn();

		bool shown = row.shown;

		ImGui::BeginDisabled(!row.yours || StageImport::IsBusy());

		if (ImGui::Checkbox("##ingame", &shown))
			StageImport::SetInGame(row.id, shown);

		ImGui::EndDisabled();

		BgGrade::Grade grade = BgGrade::Of(row.key);
		bool changed = false;

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		changed = ImGui::SliderFloat("##lift", &grade.lift, 0.0f, 0.25f, "%.2f");

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		changed = ImGui::SliderFloat("##contrast", &grade.contrast, 0.5f, 3.0f, "%.2f") || changed;

		if (changed)
			BgGrade::Set(row.key, grade);

		ImGui::TableNextColumn();

		if (row.yours)
		{
			ImGui::BeginDisabled(StageImport::IsBusy());

			if (ImGui::SmallButton("Remove"))
				StageImport::Remove(row.id);

			ImGui::EndDisabled();
			ImGui::SameLine();
		}

		ImGui::BeginDisabled(grade.lift == BgGrade::DefaultOf(row.key).lift
			&& grade.contrast == BgGrade::DefaultOf(row.key).contrast);

		if (ImGui::SmallButton("Default"))
			BgGrade::Forget(row.key);

		ImGui::EndDisabled();
		ImGui::PopID();
	}

	ImGui::EndTable();

	if (StageObjects::Loads() == 0)
		return;

	if (StageObjects::Failures() == 0)
		UiText::Muted("Object layer: %s", StageObjects::StatusText());
	else
		UiText::Warn("Object layer: %s", StageObjects::StatusText());
}

void StagesPanel::DrawHelp()
{
	ImGui::SeparatorText("Adding a stage");

	UiText::Muted("Take one out of MELTY BLOOD: TYPE LUMINA, UNI[st], UNI[cl-r], UNI Exe:Late or "
		"DFCI, or import a folder you made in Blender. Nothing is downloaded and nothing the game "
		"ships is replaced.");

	ImGui::SeparatorText("Hidden stages");

	UiText::Muted("The game builds two stages it leaves off every list - the altar it brought over "
		"from UNI, and its own debug stage. Ticking one puts it back on the list.");

	ImGui::SeparatorText("The picker");

	UiText::Muted("Every stage you install stays in Mods\\bg for good, and there is no limit on "
		"how many you keep. The picker is the part with a limit: it holds %d of your stages at a "
		"time, and Enabled is which ones sit in it. Past that a new stage still installs, unticked "
		"- untick one you are not playing and tick that one in. Nothing is deleted either way; "
		"Remove is the one that deletes files.", StageLibrary::SlotBudget());

	ImGui::SeparatorText("Restarting");

	UiText::Muted("The game reads its stage list once, on the way in, so a stage that has just "
		"arrived or left needs the restart button at the bottom. Colour is the exception and "
		"applies at once.");

	ImGui::SeparatorText("Lift and Contrast");

	UiText::Muted("Lift is flat light added to the whole stage - raise it and the stage washes "
		"out, lower it and the blacks deepen. Contrast scales the colour on top of that, which is "
		"what brightens a dark stage without washing out its blacks. Both apply mid-match, and "
		"Default puts one stage back. DFCI ports start on different numbers on purpose - that is "
		"what matches their colours to DFCI.");

	ImGui::SeparatorText("The list");

	UiText::Muted("A green name is the stage playing right now. The game's own stages can be "
		"recoloured but not removed.");

	ImGui::SeparatorText("Online");

	UiText::Muted("Stages are picture only, so the other player does not need yours, and colour is "
		"never sent.");

	ImGui::SeparatorText("Blender");

	UiText::Muted("The add-on that opens and saves a stage is in the mod's source repository, "
		"under resource\\blender, with a README next to it.");
}

void StagesPanel::DrawRestart()
{
	if (!StageImport::NeedsRestart())
		return;

	ImGui::Separator();
	UiText::Warn("The game reads its stage list once, on the way in, so what you changed here "
		"reaches the picker after a restart.");

	if (!GameRestart::CanSoftReset())
	{
		UiText::Muted("%s", GameRestart::StatusText());
		return;
	}

	ImGui::BeginDisabled(GameRestart::IsPending());

	if (ImGui::Button("Restart the game"))
		GameRestart::SoftReset();

	ImGui::EndDisabled();
}

