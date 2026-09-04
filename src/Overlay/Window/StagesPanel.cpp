#include "Overlay/Window/StagesPanel.h"

#include "Game/ExtraStages.h"
#include "Game/GameRestart.h"
#include "Game/StageImport.h"
#include "Game/StageObjects.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr float kListHeight = 220.0f;
constexpr float kNumberColumn = 60.0f;
constexpr float kSizeColumn = 80.0f;
constexpr float kActionColumn = 90.0f;
constexpr float kCheckboxGap = 24.0f;

void Megabytes(uint32_t bytes, char* out, size_t size)
{
	sprintf_s(out, size, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

}

void StagesPanel::Draw()
{
	DrawHidden();

	ImGui::Separator();
	DrawSource();

	ImGui::Separator();
	DrawPorted();

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
	UiText::Help("Pick the folder that holds MBTL.exe, UNIclr.exe or UNIst.exe. Those games store "
		"a stage the same way UNI2 does, so the mod reads one out of your own copy and installs it "
		"as a stage of its own - it does not replace anything the game ships. Nothing is "
		"downloaded. MBAACC is not offered: its backgrounds are sprite layers, not models.");

	ImGui::BeginDisabled(StageImport::IsBusy());

	if (ImGui::Button("Get stages from French-Bread games"))
		m_sourceDialog.BeginFolder("Pick the game folder to take a stage from");

	ImGui::EndDisabled();

	std::string picked;

	if (m_sourceDialog.TakeResult(picked))
	{
		m_chosen = -1;
		m_name[0] = 0;
		StageImport::Scan(picked.c_str());
	}

	if (StageImport::IsBusy())
	{
		ImGui::ProgressBar(StageImport::Progress() / 100.0f, ImVec2(Ui::Scaled(240.0f), 0.0f));
		UiText::Warn("%s", StageImport::StatusText());
		return;
	}

	UiText::Muted("%s", StageImport::StatusText());
	DrawOffers();
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

	if (index == m_chosen)
	{
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##name", m_name, sizeof(m_name));
	}
	else
	{
		ImGui::TextUnformatted(offer->name.empty() ? "unnamed" : offer->name.c_str());
	}

	ImGui::TableNextColumn();
	char size[32] = {};
	Megabytes(offer->bytes, size, sizeof(size));
	ImGui::TextUnformatted(size);

	ImGui::TableNextColumn();

	if (index != m_chosen)
	{
		if (ImGui::SmallButton("Add"))
		{
			m_chosen = index;
			strncpy_s(m_name, offer->name.empty() ? offer->folder.c_str() : offer->name.c_str(),
				_TRUNCATE);
		}

		return;
	}

	if (ImGui::SmallButton("Install"))
	{
		StageImport::Install(index, m_name);
		m_chosen = -1;
	}

	ImGui::SameLine();

	if (ImGui::SmallButton("x"))
		m_chosen = -1;
}

void StagesPanel::DrawPorted()
{
	UiText::Muted("Stages you have ported");

	if (StageImport::PortCount() == 0)
	{
		ImGui::TextDisabled("None yet.");
		return;
	}

	if (!ImGui::BeginTable("##stageports", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kNumberColumn));
	ImGui::TableSetupColumn("Name");
	ImGui::TableSetupColumn("From");
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kActionColumn));
	ImGui::TableHeadersRow();

	for (int i = 0; i < StageImport::PortCount(); ++i)
	{
		const StageImport::Port* const port = StageImport::PortAt(i);

		if (port == nullptr)
			continue;

		ImGui::PushID(port->number);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text("%d", port->number);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(port->name.c_str());

		ImGui::TableNextColumn();
		ImGui::Text("%s %s", port->game.c_str(), port->folder.c_str());

		ImGui::TableNextColumn();
		ImGui::BeginDisabled(StageImport::IsBusy());

		if (ImGui::SmallButton("Remove"))
			StageImport::Remove(port->number);

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
