#include "Overlay/Window/StagesPanel.h"

#include "Game/BgGrade.h"
#include "Game/ExtraStages.h"
#include "Game/FbGameFolder.h"
#include "Game/GameRestart.h"
#include "Game/StageImport.h"
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

const ImVec4 kPlayingText(0.45f, 0.90f, 0.50f, 1.0f);

void Megabytes(uint32_t bytes, char* out, size_t size)
{
	sprintf_s(out, size, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

struct Listed
{
	int number;
	std::string name;
	std::string from;
	bool ported;
};

void Gather(std::vector<Listed>& out)
{
	out.clear();

	for (int i = 0; i < ExtraStages::StageCount(); ++i)
	{
		const ExtraStages::Stage* const stage = ExtraStages::StageAt(i);

		if (stage == nullptr)
			continue;

		const bool free = stage->number >= StageImport::kFirstNumber &&
			stage->number <= StageImport::kLastNumber;

		Listed row = {};
		row.number = stage->number;
		row.name = stage->name.empty() ? stage->folder : stage->name;
		row.from = std::string(free ? "yours, bg\\" : "the game's own, bg\\") + stage->folder;
		row.ported = false;

		out.push_back(row);
	}

	for (int i = 0; i < StageImport::PortCount(); ++i)
	{
		const StageImport::Port* const port = StageImport::PortAt(i);

		if (port == nullptr)
			continue;

		Listed row = {};
		row.number = port->number;
		row.name = port->name;
		row.from = port->game + " " + port->folder;
		row.ported = true;

		const auto same = std::find_if(out.begin(), out.end(),
			[&row](const Listed& known) { return known.number == row.number; });

		if (same != out.end())
			*same = row;
		else
			out.push_back(row);
	}

	std::sort(out.begin(), out.end(),
		[](const Listed& a, const Listed& b) { return a.number < b.number; });
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
	UiText::Muted("Stages the game builds and hides");
	UiText::Help("The game ships two stages it leaves off every list: the altar it brought over "
		"from UNI, and its own debug stage. Both are complete and play like any other - the "
		"picker just never offers them, because the number is missing from the list it is built "
		"from. Ticking one puts it back on that list, which the game reads once on the way in, so "
		"it appears after a restart.");

	if (!ExtraStages::Ready())
	{
		ImGui::TextDisabled("Reading the game's stage list...");
		return;
	}

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
	UiText::Help("Pick the folder holding MBTL.exe, UNIclr.exe, UNIst.exe or DFCI. The stage is "
		"copied out of your own install into a free stage number. Nothing the game ships is "
		"replaced and nothing is downloaded. MBAACC is not offered - its backgrounds are 2D.");

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
	UiText::Help("Pick a folder holding bg.fbx.bin, its .dds textures and a stage.txt naming the "
		"stage. That is what Blender produces, and what every installed stage looks like in "
		"Mods\\bg - copy one out, edit it, bring it back.");

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

	if (StageImport::FreeNumber() < 0)
	{
		UiText::Warn("Every stage number the mod may use is taken. Remove a port first.");
		return;
	}

	if (ImGui::Button("Add all"))
	{
		for (int i = 0; i < StageImport::OfferCount(); ++i)
			Queue(i);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Installs every stage listed, under the names shown, several at a "
			"time. It stops early if the mod runs out of stage numbers.");
	}

	ImGui::SameLine();
	UiText::Muted("Edit a name first if you want to - Add installs straight away.");

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

void StagesPanel::DrawPorted()
{
	UiText::Muted("Every stage this build has");

	if (ExtraStages::StageCount() == 0 && StageImport::PortCount() == 0)
	{
		ImGui::BeginChild("##noports", ImVec2(0.0f, Ui::Scaled(kPortedHeight)), false);
		ImGui::TextDisabled("None yet.");
		ImGui::EndChild();
		return;
	}

	if (!ImGui::BeginTable("##stageports", 5,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(kPortedHeight))))
	{
		return;
	}

	ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kNumberColumn));
	ImGui::TableSetupColumn("Name");
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
		ImGui::PushID(row.number);
		ImGui::TableNextRow();

		const bool inMatch = row.number == playing;

		ImGui::TableNextColumn();
		ImGui::Text("%d", row.number);

		ImGui::TableNextColumn();

		if (inMatch)
			ImGui::TextColored(kPlayingText, "%s", row.name.c_str());
		else
			ImGui::TextUnformatted(row.name.c_str());

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", row.from.c_str());

		BgGrade::Grade grade = BgGrade::Of(row.number);
		bool changed = false;

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		changed = ImGui::SliderFloat("##lift", &grade.lift, 0.0f, 0.25f, "%.2f");

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		changed = ImGui::SliderFloat("##contrast", &grade.contrast, 0.5f, 3.0f, "%.2f") || changed;

		if (changed)
			BgGrade::Set(row.number, grade);

		ImGui::TableNextColumn();

		if (row.ported)
		{
			ImGui::BeginDisabled(StageImport::IsBusy());

			if (ImGui::SmallButton("Remove"))
				StageImport::Remove(row.number);

			ImGui::EndDisabled();
			ImGui::SameLine();
		}

		ImGui::BeginDisabled(grade.lift == BgGrade::DefaultOf(row.number).lift
			&& grade.contrast == BgGrade::DefaultOf(row.number).contrast);

		if (ImGui::SmallButton("Default"))
			BgGrade::Forget(row.number);

		ImGui::EndDisabled();
		ImGui::PopID();
	}

	ImGui::EndTable();

	UiText::Help("Colour is live: drag either one during a match and the background changes under "
		"you. The game draws a background as lift + texture * vertex colour * contrast, so Lift is "
		"a flat amount added to every pixel - raising it makes the whole stage brighter and "
		"flatter, and lowering it deepens the blacks. Contrast multiplies, which is the one that "
		"makes a dark stage brighter without washing out its blacks: a vertex colour cannot go "
		"above 1, and this is where the rest of the range lives. UNI2 adds 0.10 to its own stages "
		"and paints them for it; DFCI adds nothing and draws its textures as they are, so a DFCI "
		"port starts at 0.00 lift with the contrast that puts the two games' output back on top of "
		"each other.");

	if (!BgGrade::Reached())
		UiText::Warn("Colour: %s", BgGrade::StatusText());

	if (StageObjects::Loads() == 0)
		return;

	if (StageObjects::Failures() == 0)
		UiText::Muted("Object layer: %s", StageObjects::StatusText());
	else
		UiText::Warn("Object layer: %s", StageObjects::StatusText());
}

void StagesPanel::DrawHelp()
{
	ImGui::SeparatorText("The two tabs");

	UiText::Muted("Installed Stages is everything you can pick in the game. Add stages is where "
		"new ones come from.");

	ImGui::Spacing();
	ImGui::SeparatorText("Adding a stage");

	UiText::Muted("From another French-Bread game you own: MELTY BLOOD: TYPE LUMINA, UNI[st], "
		"UNI[cl-r] or DFCI. Or import a folder you made in Blender.");

	UiText::Muted("Nothing is downloaded and nothing the game ships is replaced.");

	ImGui::Spacing();
	ImGui::SeparatorText("New stages need a restart");

	UiText::Muted("The game reads its stage list once, at startup. A button appears at the bottom "
		"when a restart is due. Colour is the exception and applies at once.");

	ImGui::Spacing();
	ImGui::SeparatorText("Lift and Contrast");

	UiText::Muted("Lift is flat light added to the whole stage. Raise it and the stage washes out, "
		"lower it and the blacks deepen.");

	UiText::Muted("Contrast scales the colour on top of that.");

	UiText::Muted("Both apply immediately, mid-match included. Default puts one stage back, so "
		"nothing you do here can be lost.");

	UiText::Muted("DFCI ports start on different numbers than the rest. That is deliberate - it is "
		"what matches their colours to DFCI.");

	ImGui::Spacing();
	ImGui::SeparatorText("The list");

	UiText::Muted("A green name is the stage playing right now.");
	UiText::Muted("Remove only appears on stages you added. The game's own can be recoloured but "
		"not removed.");

	ImGui::Spacing();
	ImGui::SeparatorText("Online");

	UiText::Muted("Stages are picture only, so they change nothing about how a match plays and the "
		"other player does not need yours. Colour is never sent.");

	ImGui::Spacing();
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

