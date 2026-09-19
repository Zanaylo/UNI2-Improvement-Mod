#include "Overlay/Window/StagesPanel.h"

#include "Game/BgGrade.h"
#include "Game/FbGameFolder.h"
#include "Game/BgMipmaps.h"
#include "Game/ModFiles.h"
#include "Game/StagePlacement.h"
#include "Game/ExtraStages.h"
#include "Game/GameRestart.h"
#include "Game/OnlineStage.h"
#include "Game/BgCeiling.h"
#include "Game/StageImport.h"
#include "Game/StageLibrary.h"
#include "Game/StageObjects.h"
#include "Game/StageReplacements.h"
#include "Core/interfaces.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"

#include <imgui.h>
#include <imgui_internal.h>

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
constexpr float kPortedHeight = 330.0f;
constexpr float kGradeColumn = 78.0f;
constexpr float kInGameColumn = 70.0f;
constexpr float kPlacementWidth = 260.0f;
constexpr float kLeastScale = 0.05f;
constexpr float kMostScale = 400.0f;
constexpr float kScaleFloor = 0.1f;
constexpr float kScaleCeiling = 4.0f;
constexpr float kMostMove = 40.0f;

constexpr float kMostGlow = 2.0f;

const ImVec4 kPlayingText(0.45f, 0.90f, 0.50f, 1.0f);

void Resize(StagePlacement::Place& place, float size)
{
	const float was = place.scale[0];

	if (was <= 0.0f)
	{
		for (int i = 0; i < 3; ++i)
			place.scale[i] = size;

		return;
	}

	const float by = size / was;

	for (int i = 0; i < 3; ++i)
		place.scale[i] *= by;
}

void ScaleRange(int stage, float& low, float& high)
{
	StagePlacement::Place shipped = {};

	if (!StagePlacement::Shipped(stage, shipped) || shipped.scale[0] <= 0.0f)
	{
		low = kLeastScale;
		high = kMostScale;
		return;
	}

	low = shipped.scale[0] * kScaleFloor;
	high = shipped.scale[0] * kScaleCeiling;
}

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
	bool replaced;
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

		std::string replacement;
		row.replaced = StageReplacements::Replaced(stage->number, replacement);

		if (row.replaced)
		{
			char place[160] = {};
			sprintf_s(place, "in place of %s, Mods\\bg\\bg%03d", row.name.c_str(), stage->number);

			row.from = place;
			row.name = replacement.empty() ? row.name : replacement;
		}

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

bool StagesPanel::Number(const char* id, float* value, float low, float high,
	const char* format)
{
	ImGui::SetNextItemWidth(-1.0f);

	const float was = *value;
	const bool dragged = ImGui::SliderFloat(id, value, low, high, format);
	const ImGuiID key = ImGui::GetItemID();

	if (ImGui::IsItemActivated() && ImGui::GetIO().MouseClickedCount[ImGuiMouseButton_Left] < 2)
	{
		m_pressed = key;
		m_pressedValue = was;
	}

	if (!ImGui::IsItemHovered() || !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) ||
		ImGui::TempInputIsActive(key))
	{
		return dragged;
	}

	*value = m_pressed == key ? m_pressedValue : was;

	ImGui::ClearActiveID();
	ImGui::ActivateItemByID(key);
	GImGui->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;

	return true;
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
		if (ImGui::BeginTabItem("Installed stages"))
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

			ImGui::Separator();
			DrawReplace();

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
		ImGui::TextDisabled("This game version has no hidden stages.");
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

	if (ImGui::Button("Get stages from another fighting game"))
		m_sourceDialog.BeginFolder("Pick the game folder to take a stage from");

	ImGui::EndDisabled();

	ImGui::SameLine();
	UiText::Help("Pick the folder a game is installed in and the mod reads its stage data. "
		"Nothing is downloaded and no UNI2 file is replaced."
		"\n\nFrench-Bread: UNI[st], UNI[cl-r], UNI Exe:Late, MELTY BLOOD: TYPE LUMINA, MELTY "
		"BLOOD Actress Again Current Code and DENGEKI BUNKO FIGHTING CLIMAX IGNITION."
		"\n\nArc System Works: BLAZBLUE CROSS TAG BATTLE and BLAZBLUE CENTRALFICTION, every "
		"stage of both.");

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
	DrawArcsys();
	DrawOffers();
}

void StagesPanel::DrawArcsys()
{
	const FbGameFolder::Game game = StageImport::ScannedKind();

	if (game != FbGameFolder::Game_BBTAG && game != FbGameFolder::Game_BBCF)
		return;

	UiText::Muted("These stages convert here, but the Mua add-on for Blender does a better job. It "
		"reads the skeleton, the motions, the scripts and every sprite sheet. Here only one sheet is "
		"used, so animated sprites stand still. Import what you make in Blender with the button "
		"below.");

	if (game != FbGameFolder::Game_BBCF)
		return;

	UiText::Warn("CENTRALFICTION stages can come out at the wrong size. If one does, set Size by "
		"eye.");
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

void StagesPanel::DrawReplace()
{
	UiText::Muted("In place of one of the game's stages");

	if (ExtraStages::StageCount() == 0)
	{
		ImGui::TextDisabled("The game has not read its stage list yet.");
		return;
	}

	const auto titled = [](const ExtraStages::Stage& stage)
	{
		char title[160] = {};
		sprintf_s(title, "%d  %s", stage.number, stage.name.empty() ? stage.folder.c_str() : stage.name.c_str());

		return std::string(title);
	};

	std::vector<const ExtraStages::Stage*> own;
	std::string preview = "Pick a stage";

	for (int i = 0; i < ExtraStages::StageCount(); ++i)
	{
		const ExtraStages::Stage* const stage = ExtraStages::StageAt(i);

		if (stage == nullptr || stage->number <= 0 || !StageLibrary::GameOwns(stage->number))
			continue;

		own.push_back(stage);

		if (stage->number == m_replaceNumber)
			preview = titled(*stage);
	}

	ImGui::SetNextItemWidth(Ui::Scaled(kPlacementWidth));

	if (ImGui::BeginCombo("##replace", preview.c_str()))
	{
		for (const ExtraStages::Stage* stage : own)
		{
			if (ImGui::Selectable(titled(*stage).c_str(), stage->number == m_replaceNumber))
				m_replaceNumber = stage->number;
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(StageImport::IsBusy() || m_replaceDialog.IsRunning() || m_replaceNumber <= 0);

	if (ImGui::Button("Replace it with a stage folder"))
		m_replaceDialog.BeginFolder("Pick the folder holding the stage's files");

	ImGui::EndDisabled();

	ImGui::SameLine();
	UiText::Help("The stage keeps its number, card, walls and selection flags, so it needs no free "
		"number. Only the look changes. An opponent without it sees the game's own stage."
		"\n\nRestore, under Installed stages, brings the game's stage back.");

	std::string picked;

	if (m_replaceDialog.TakeResult(picked))
		StageImport::ReplaceFolder(picked.c_str(), m_replaceNumber);
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
		ImGui::SetTooltip("Installs every stage in the list with the names shown. Stages that don't "
			"fit in the picker go into your library unticked.");
	}

	ImGui::SameLine();
	UiText::Muted("You can edit a name before adding it.");

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

	if (BgGrade::Reached())
		UiText::Muted("Colour: %s.", BgGrade::StatusText());
	else
		UiText::Warn("Colour: %s", BgGrade::StatusText());

	const int scrolling = BgGrade::Scrolling();

	if (scrolling != 0)
		UiText::Muted("%d surface(s) of this stage scroll.", scrolling);

	UiText::Muted("Mipmaps: %s.", BgMipmaps::StatusText());
	UiText::Muted("Loading: %s.", ModFiles::LoadText());

	UiText::Muted("%d/%d stages.", StageLibrary::Total(), BgCeiling::Numbers());

	if (StageLibrary::ShownCount() >= budget)
		UiText::Warn("The picker is full at %d stage(s). Untick one to make room.", budget);

	DrawPlacement();
}

void StagesPanel::DrawPlacement()
{
	if (!g_modVals.advancedStages)
		return;

	const int stage = StagePlacement::Current();

	StagePlacement::Place place = {};

	if (stage < 0 || !StagePlacement::Of(stage, place))
		return;

	if (!ImGui::CollapsingHeader("Stage scale and position"))
		return;

	UiText::Muted("Scale makes the stage bigger around the fight. Position moves it. The fighters "
		"stay where they are, so a bigger scale makes them look smaller.");

	bool changed = false;

	float low = kLeastScale;
	float high = kMostScale;
	ScaleRange(stage, low, high);

	ImGui::PushItemWidth(Ui::Scaled(kPlacementWidth));

	changed |= ImGui::SliderFloat3("Scale##placescale", place.scale, low, high, "%.2f");
	changed |= ImGui::SliderFloat3("Position##placemove", place.position, -kMostMove, kMostMove,
		"%.2f");

	ImGui::PopItemWidth();

	if (changed)
		StagePlacement::Set(stage, place);

	if (StagePlacement::Edited(stage))
	{
		ImGui::SameLine();

		if (ImGui::Button("Reset##placereset"))
			StagePlacement::Forget(stage);
	}

	UiText::Muted("%s.", StagePlacement::StatusText());
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

	const bool advanced = g_modVals.advancedStages != 0;

	if (!ImGui::BeginTable("##stageports", advanced ? 9 : 7,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(kPortedHeight))))
	{
		return;
	}

	ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kNumberColumn));
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f);
	ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed,
		Ui::Scaled(kInGameColumn));
	ImGui::TableSetupColumn("Colour", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kInGameColumn));
	ImGui::TableSetupColumn("Lift", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kGradeColumn));
	ImGui::TableSetupColumn("Contrast", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kGradeColumn));

	if (advanced)
	{
		ImGui::TableSetupColumn("Light", ImGuiTableColumnFlags_WidthFixed,
			Ui::Scaled(kGradeColumn));
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed,
			Ui::Scaled(kGradeColumn));
	}

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

		ImGui::TableNextColumn();

		bool graded = !BgGrade::IsOff(row.key);

		if (ImGui::Checkbox("##graded", &graded))
			BgGrade::SetOff(row.key, !graded);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Untick to show this stage in the game's own colour, without Lift, "
				"Contrast or Light. Your values are kept for when you tick it again.");
		}

		BgGrade::Grade grade = BgGrade::Of(row.key);
		bool changed = false;

		ImGui::BeginDisabled(!graded);

		ImGui::TableNextColumn();
		changed = Number("##lift", &grade.lift, 0.0f, 0.25f, "%.2f");

		ImGui::TableNextColumn();
		changed = Number("##contrast", &grade.contrast, 0.5f, 3.0f, "%.2f") || changed;

		ImGui::EndDisabled();

		if (changed)
			BgGrade::Set(row.key, grade);

		float glow = BgGrade::GlowOf(row.key);
		StagePlacement::Place place = {};
		const bool placed = row.slot >= 0 && StagePlacement::Of(row.slot, place);

		if (advanced)
		{
			ImGui::TableNextColumn();

			ImGui::BeginDisabled(!graded);

			if (Number("##glow", &glow, 0.0f, kMostGlow, "%.2f"))
				BgGrade::SetGlow(row.key, glow);

			ImGui::EndDisabled();

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Changes only the glowing parts of this stage, like lamps, glows "
					"and flares. The rest keeps its brightness. UNI2's own stages have none, so this "
					"does nothing on them.");
			}

			ImGui::TableNextColumn();

			if (!placed)
			{
				ImGui::TextDisabled("-");
			}
			else
			{
				float size = place.scale[0];
				float low = kLeastScale;
				float high = kMostScale;

				ScaleRange(row.slot, low, high);

				if (Number("##size", &size, low, high, "%.2f"))
				{
					Resize(place, size);
					StagePlacement::Set(row.slot, place);
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How big the stage is around the fight. A bigger stage "
						"makes the fighters look smaller. The slider goes from %.2f to %.2f; "
						"double click it to type any number.", low, high);
				}
			}
		}

		ImGui::TableNextColumn();

		if (row.yours)
		{
			ImGui::BeginDisabled(StageImport::IsBusy());

			if (ImGui::SmallButton("Remove"))
				StageImport::Remove(row.id);

			ImGui::EndDisabled();
			ImGui::SameLine();
		}

		if (row.replaced)
		{
			ImGui::BeginDisabled(StageImport::IsBusy());

			if (ImGui::SmallButton("Restore"))
				StageImport::Restore(row.slot);

			ImGui::EndDisabled();

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("Deletes Mods\\bg\\bg%03d, so the game's own stage comes back.", row.slot);

			ImGui::SameLine();
		}

		const BgGrade::Grade untouched = BgGrade::DefaultOf(row.key);
		const bool moved = placed && StagePlacement::Edited(row.slot);

		ImGui::BeginDisabled(grade.lift == untouched.lift
			&& grade.contrast == untouched.contrast
			&& glow == BgGrade::DefaultGlowOf(row.key) && !moved);

		if (ImGui::SmallButton("Default"))
		{
			BgGrade::Forget(row.key);

			if (moved)
				StagePlacement::Forget(row.slot);
		}

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

	UiText::Muted("Take one from MELTY BLOOD: TYPE LUMINA, UNI[st], UNI[cl-r], UNI Exe:Late, DFCI, "
		"BLAZBLUE CROSS TAG BATTLE or BLAZBLUE CENTRALFICTION, or import a folder you made in "
		"Blender. Nothing is downloaded and none of the game's files are replaced.");

	ImGui::SeparatorText("Hidden stages");

	UiText::Muted("The game has two stages it never lists: the altar brought over from UNI and its "
		"own debug stage. Tick one to put it back on the list.");

	ImGui::SeparatorText("The picker");

	UiText::Muted("Installed stages stay in Mods\\bg, and you can keep as many as you like. The "
		"picker holds %d of them at a time, and Enabled chooses which. When it is full, new stages "
		"still install but unticked, so untick one you don't play and tick the new one. Only Remove "
		"deletes files.", StageLibrary::SlotBudget());

	ImGui::SeparatorText("Restarting");

	UiText::Muted("The game reads its stage list only at startup. After adding or removing a "
		"stage, use the restart button at the bottom. Colour changes apply right away.");

	ImGui::SeparatorText("Typing a number");

	UiText::Muted("Double click any slider in the list to type a value. A typed value can go past "
		"the slider's range.");

	ImGui::SeparatorText("Lift and Contrast");

	UiText::Muted("Lift adds flat light to the whole stage: raise it to wash the stage out, lower "
		"it for deeper blacks. Contrast scales the colour on top, which brightens a dark stage "
		"without washing out its blacks. Both apply during a match, and Default resets one stage. "
		"DFCI ports start with different values on purpose, to match DFCI's colours.");

	ImGui::SeparatorText("Light and Size");

	UiText::Muted("These columns only show when Advanced stage options is ticked, in the mod window "
		"under Config. Light changes only the glowing parts of a stage, and UNI2's own stages have "
		"none, so it does nothing on them. Size is how big the scenery is around the fight. Each "
		"stage has its own default size, so the slider range is based on it.");

	ImGui::SeparatorText("The list");

	UiText::Muted("A green name is the stage playing right now. The game's own stages can be "
		"recoloured but not removed.");

	ImGui::SeparatorText("Replacing a game stage");

	UiText::Muted("Add stages can put a stage folder in place of one of the game's own. It keeps the "
		"game's number, card and walls. Restore brings the game's stage back.");

	ImGui::SeparatorText("Online");

	UiText::Muted("An added stage is only on your machine, so an opponent is never offered it: they get "
		"the game's first stage while you keep playing on yours. A stage they ask for that this game "
		"does not have plays as the game's first stage here. A replaced stage keeps the game's number, "
		"so an opponent without it sees the game's own. Online, Random picks only from the game's own "
		"stages. Colour settings are never sent.");

	UiText::Muted("%s", OnlineStage::StatusText());

	ImGui::SeparatorText("Blender");

	UiText::Muted("The add-on that opens and saves a stage is in the mod's source repository, "
		"under resource\\blender, with a README next to it.");
}

void StagesPanel::DrawRestart()
{
	if (!StageImport::NeedsRestart())
		return;

	ImGui::Separator();
	UiText::Warn("The game reads its stage list only at startup. Restart to see your changes in "
		"the picker.");

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

