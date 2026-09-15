#include "Overlay/Window/PatchWindow.h"

#include "Game/BalanceRules.h"
#include "Game/GamePatches.h"
#include "Game/GameRestart.h"
#include "Game/SceneWatch.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"

#include <imgui.h>

#include <cstdio>

namespace {

constexpr float kFieldWidth = 240.0f;

void DateText(const SYSTEMTIME& time, char* out, int size)
{
	if (time.wYear == 0)
	{
		strncpy_s(out, size, "no date", _TRUNCATE);
		return;
	}

	sprintf_s(out, size, "%04d-%02d-%02d", time.wYear, time.wMonth, time.wDay);
}

bool ParseDate(const char* text, SYSTEMTIME& out)
{
	int year = 0;
	int month = 0;
	int day = 0;

	if (sscanf_s(text, "%d-%d-%d", &year, &month, &day) != 3)
		return false;

	if (year < 2000 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31)
		return false;

	out = {};
	out.wYear = static_cast<WORD>(year);
	out.wMonth = static_cast<WORD>(month);
	out.wDay = static_cast<WORD>(day);
	return true;
}

const char* NameOf(const GamePatches::Patch* patch)
{
	return patch != nullptr ? patch->name.c_str() : "the installed game";
}

}

PatchWindow::PatchWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void PatchWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(660.0f), Ui::Scaled(460.0f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(Ui::Scaled(480.0f), Ui::Scaled(300.0f)),
		ImVec2(FLT_MAX, FLT_MAX));
}

void PatchWindow::Draw()
{
	if (!GamePatches::IsSupported())
	{
		UiText::Warn("Patches are unavailable: this mod version can't find the game's data "
			"search path.");
		return;
	}

	PatchLibrary::EnsureIndexed(PatchLibrary::NextUnindexed());

	DrawActive();
	ImGui::Separator();
	DrawTable();
	ImGui::Separator();
	DrawInstall();
}

void PatchWindow::DrawActive()
{
	const GamePatches::Patch* const active = GamePatches::Get(GamePatches::ActiveIndex());

	UiText::Good("Running %s.", NameOf(active));

	if (active != nullptr)
		DrawCoverage(*active);

	DrawEngine(active);
	DrawRestart();
}

void PatchWindow::DrawRestart()
{
	const int boot = GamePatches::BootIndex();
	const int chosen = GamePatches::ChosenIndex();
	const int replay = GamePatches::ReplayWanted();

	const int wanted = chosen != boot ? chosen : (replay >= 0 && replay != boot ? replay : boot);

	if (wanted != boot)
	{
		const GamePatches::Patch* const target = GamePatches::Get(wanted);

		if (wanted == replay && chosen == boot)
			UiText::Warn("This replay was recorded on %s.", NameOf(target));
		else
			UiText::Warn("%s is picked but not loaded.", NameOf(target));

		UiText::Muted("A patch only loads when the game restarts.");
	}

	if (chosen != boot)
	{
		UiText::Muted("Offline Training doesn't need a restart. Entering it switches the battle data "
			"to %s, and leaving it switches back.", NameOf(GamePatches::Get(chosen)));
	}

	char label[96] = {};
	sprintf_s(label, "Reload with %s", NameOf(GamePatches::Get(wanted)));

	ImGui::BeginDisabled(!GameRestart::CanSoftReset());

	if (ImGui::Button(label))
	{
		GamePatches::ApplyForReset(wanted);
		GameRestart::SoftReset();
	}

	ImGui::EndDisabled();

	UiText::Help("Sends the game back to its first loading screen, so it loads the patch like a "
		"fresh launch. In training it leaves the battle first.");

	if (GameRestart::StatusText()[0] != '\0')
		UiText::Muted("%s", GameRestart::StatusText());

	if (GamePatches::HomeIndex() >= 0)
	{
		UiText::Warn("Online: %s is only used in player matches where the other player has the mod "
			"and the same patch. Ranked, casual and everyone else get the installed game, "
			"switched before the match starts.", NameOf(GamePatches::Get(GamePatches::HomeIndex())));
	}

	bool automatic = GamePatches::IsAuto();

	if (ImGui::Checkbox("Find the patch each replay was recorded on", &automatic))
		GamePatches::SetAuto(automatic);
}

void PatchWindow::DrawCoverage(const GamePatches::Patch& patch)
{
	if (!patch.indexed)
	{
		UiText::Muted("Reading the folder...");
		return;
	}

	const GamePatches::Coverage& coverage = patch.coverage;

	UiText::Muted("%d file(s), %d of %d characters.", coverage.files, coverage.characters,
		coverage.charactersWanted);

	if (coverage.characters < coverage.charactersWanted)
		UiText::Warn("Missing characters use data from a newer version.");
}

void PatchWindow::DrawEngine(const GamePatches::Patch* patch)
{
	if (BalanceRules::Count() == 0 || patch == nullptr)
		return;

	if (patch->version == 0)
	{
		UiText::Warn("The name has no version number, so engine values stay as they are.");
		return;
	}

	for (int i = 0; i < BalanceRules::Count(); ++i)
	{
		const BalanceRules::Rule* const rule = BalanceRules::Get(i);

		if (BalanceRules::IsActive(i))
			UiText::Good("%s: on", rule->name);
		else
			UiText::Muted("%s: off, the game adds it in %d.%02d", rule->name, rule->since / 100,
				rule->since % 100);
	}
}

void PatchWindow::DrawInstalledRow()
{
	const bool running = GamePatches::ActiveIndex() < 0;
	const bool picked = GamePatches::ChosenIndex() < 0;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	if (running)
		UiText::Good("The installed game");
	else
		ImGui::TextUnformatted("The installed game");

	ImGui::TableNextColumn();
	UiText::Muted("the game as installed");

	ImGui::TableNextColumn();
	UiText::Muted("current");

	ImGui::TableNextColumn();
	UiText::Muted("all");

	ImGui::TableNextColumn();

	if (running)
		UiText::Good("running");
	else if (picked)
		UiText::Warn("on restart");
	else
		UiText::Muted("ready");

	ImGui::TableNextColumn();

	ImGui::BeginDisabled(picked);

	if (ImGui::SmallButton("Use"))
		GamePatches::Choose(-1);

	ImGui::EndDisabled();
}

void PatchWindow::DrawRow(int index)
{
	const GamePatches::Patch* const patch = GamePatches::Get(index);

	if (patch == nullptr)
		return;

	const bool running = index == GamePatches::ActiveIndex();
	const bool picked = index == GamePatches::ChosenIndex();

	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	if (running)
		UiText::Good("%s", patch->name.c_str());
	else if (patch->present)
		ImGui::TextUnformatted(patch->name.c_str());
	else
		UiText::Muted("%s", patch->name.c_str());

	ImGui::TableNextColumn();
	UiText::Muted("%s", patch->note.c_str());

	ImGui::TableNextColumn();

	char edit[32] = {};
	DateText(patch->released, edit, sizeof(edit));

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##date", edit, IM_ARRAYSIZE(edit));

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		SYSTEMTIME parsed = {};

		if (ParseDate(edit, parsed))
			GamePatches::SetDate(index, parsed);
		else
			strncpy_s(m_status, "type a date like 2025-08-18", _TRUNCATE);
	}

	ImGui::TableNextColumn();

	if (!patch->present)
		UiText::Warn("-");
	else if (patch->coverage.characters >= patch->coverage.charactersWanted)
		UiText::Good("%d/%d", patch->coverage.characters, patch->coverage.charactersWanted);
	else
		UiText::Warn("%d/%d", patch->coverage.characters, patch->coverage.charactersWanted);

	ImGui::TableNextColumn();

	if (!patch->present)
		UiText::Warn("missing");
	else if (running)
		UiText::Good("running");
	else if (picked)
		UiText::Warn("on restart");
	else
		UiText::Muted("ready");

	ImGui::TableNextColumn();

	ImGui::BeginDisabled(!patch->present || picked);

	if (ImGui::SmallButton("Use"))
		GamePatches::Choose(index);

	ImGui::EndDisabled();
}

void PatchWindow::DrawTable()
{
	if (!ImGui::BeginTable("##patches", 6,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_ScrollY, ImVec2(0.0f, Ui::Scaled(190.0f))))
	{
		return;
	}

	ImGui::TableSetupColumn("Patch", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(84.0f));
	ImGui::TableSetupColumn("Notes");
	ImGui::TableSetupColumn("Released", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(100.0f));
	ImGui::TableSetupColumn("Cast", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(56.0f));
	ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(70.0f));
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(46.0f));
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	ImGui::PushID("installed");
	DrawInstalledRow();
	ImGui::PopID();

	for (int i = 0; i < GamePatches::Count(); ++i)
	{
		ImGui::PushID(i);
		DrawRow(i);
		ImGui::PopID();
	}

	ImGui::EndTable();

	if (GamePatches::Count() == 0)
		UiText::Muted("No patches added yet.");


}

void PatchWindow::DrawInstall()
{
	UiText::Muted("Add a patch");

	Ui::SetItemWidth(kFieldWidth);
	ImGui::InputTextWithHint("Name", "1.05", m_name, IM_ARRAYSIZE(m_name));

	if (ImGui::Button("Pick a folder"))
		m_folderDialog.BeginFolder("Pick the patch's data folder");

	std::string picked;

	if (m_folderDialog.TakeResult(picked))
	{
		if (GamePatches::Import(picked, m_name, m_status, sizeof(m_status)))
			m_name[0] = '\0';
	}

	UiText::Help("Pick the folder that holds data and script. Name the patch after its version "
		"so the engine values match it.");

	if (m_status[0] != '\0')
		UiText::Muted("%s", m_status);
}
