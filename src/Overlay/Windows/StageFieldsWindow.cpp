#include "Overlay/Windows/StageFieldsWindow.h"

#include "Core/ShellOpen.h"
#include "Game/Stages/StageExport.h"
#include "Game/Stages/StageImport.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Stages/StagePlacement.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

enum class Kind
{
	Toggle,
	Whole,
	Real,
	Colour,
};

constexpr float kWindowWidth = 460.0f;
constexpr float kWindowHeight = 620.0f;
constexpr float kFieldWidth = 220.0f;
constexpr float kMostMove = 40.0f;
constexpr float kMoveSpeed = 0.05f;
constexpr float kScaleSpeed = 0.05f;
constexpr float kMostScale = 200.0f;
constexpr float kFovSpeed = 0.1f;
constexpr float kHorizonSpeed = 0.0005f;
constexpr float kMostHorizon = 2.0f;
constexpr float kAngleSpeed = 0.1f;
constexpr float kMostTilt = 90.0f;
constexpr float kMostTurn = 180.0f;
constexpr int kColourParts = 4;

struct Field
{
	const char* key;
	const char* label;
	const char* group;
	Kind kind;
	float least;
	float most;
	float speed;
	const char* help;
};

const Field kFields[] = {
	{ "ViewGrid", "Show grid", "View", Kind::Toggle, 0, 1, 0, "Draws the game's debug grid over the stage." },
	{ "IsFog", "Fog", "Fog", Kind::Toggle, 0, 1, 0, "Fades the far scenery into the fog colour." },
	{ "FogStart", "Fog start", "Fog", Kind::Real, 0, 10000, 1, "How far away the fog begins." },
	{ "FogEnd", "Fog end", "Fog", Kind::Real, 0, 10000, 1, "How far away the fog is solid." },
	{ "FogColor", "Fog colour", "Fog", Kind::Colour, 0, 1, 0, "The colour far scenery fades into." },
	{ "MSAA", "MSAA", "Quality", Kind::Whole, 4, 8, 0.05f, "Antialiasing samples. 4 is what the game's own stages use." },
	{ "IsBloom", "Bloom", "Light", Kind::Toggle, 0, 1, 0, "The game's bloom on the whole screen." },
	{ "ShadowLightType", "Shadow light type", "Shadows", Kind::Whole, 0, 3, 0.05f, "Which light casts the fighters' shadows." },
	{ "ShadowReflexColor", "Shadow reflection colour", "Shadows", Kind::Colour, 0, 1, 0, "The colour reflected into the shadows." },
	{ "ShadowScale", "Shadow size", "Shadows", Kind::Real, 0, 2, 0.01f, "How big the fighters' shadows are." },
	{ "ShadowAlpha", "Shadow strength", "Shadows", Kind::Real, 0, 1, 0.01f, "How dark the fighters' shadows are." },
	{ "BGBloomEnable", "Stage bloom", "Stage bloom", Kind::Toggle, 0, 1, 0, "Glow on the bright parts of the stage." },
	{ "BGBloomBlightness", "Brightness", "Stage bloom", Kind::Real, 0, 4, 0.01f, "How bright a part has to be to glow." },
	{ "BGBloomPower", "Power", "Stage bloom", Kind::Real, 0, 10, 0.01f, "How strong the glow is." },
	{ "BGBloomBiassR", "Red", "Stage bloom", Kind::Real, 0, 4, 0.01f, "How much red the glow keeps." },
	{ "BGBloomBiassG", "Green", "Stage bloom", Kind::Real, 0, 4, 0.01f, "How much green the glow keeps." },
	{ "BGBloomBiassB", "Blue", "Stage bloom", Kind::Real, 0, 4, 0.01f, "How much blue the glow keeps." },
	{ "BGBloomBlurRadius", "Blur radius", "Stage bloom", Kind::Real, 0, 8, 0.01f, "How far the glow spreads." },
	{ "BGBloomTextureSize", "Texture size", "Stage bloom", Kind::Whole, 64, 256, 1, "Size of the glow buffer. 256 is the most the game's own stages use." },
	{ "BGBloomAlpha", "Opacity", "Stage bloom", Kind::Real, 0, 1, 0.01f, "How much of the glow is laid over the stage." },
	{ "BGTinyFXAAEnable", "FXAA", "Smoothing", Kind::Toggle, 0, 1, 0, "Smooths the stage's jagged edges." },
	{ "BGTinyFXAAThreshold", "Threshold", "Smoothing", Kind::Real, 0, 1, 0.005f, "How sharp an edge has to be to be smoothed." },
	{ "BGTinyFXAALerpT", "Strength", "Smoothing", Kind::Real, 0, 1, 0.005f, "How much an edge is smoothed." },
};

constexpr int kFieldCount = static_cast<int>(sizeof(kFields) / sizeof(kFields[0]));

struct ExportGame
{
	FbGameFolder::Game game;
	const char* label;
};

constexpr ExportGame kExportGames[] = { { FbGameFolder::Game_BBTAG, "BBTAG" }, { FbGameFolder::Game_BBCF, "BBCF" } };

const char* LabelOf(FbGameFolder::Game game)
{
	for (const ExportGame& one : kExportGames)
	{
		if (one.game == game)
			return one.label;
	}

	return "";
}

std::string TargetLabel(FbGameFolder::Game game, const StageInstall::Target& target)
{
	const size_t slash = target.folder.find_last_of('\\');
	std::string out = target.stem + " (" + target.folder.substr(slash + 1) + ")";
	const std::string installed = StageInstall::Installed(game, target);

	if (!installed.empty())
		out += ", now " + installed;

	return out;
}

int Numbers(const std::string& text, float* out, int wanted)
{
	const char* at = text.c_str();
	int found = 0;

	while (*at != 0 && found < wanted)
	{
		while (*at != 0 && strchr("[], \t\r\n", *at) != nullptr)
			++at;

		char* end = nullptr;
		const float value = strtof(at, &end);

		if (end == at)
			break;

		out[found++] = value;
		at = end;
	}

	return found;
}

std::string Real(float value)
{
	char text[32] = {};
	sprintf_s(text, "%.4f", value);

	std::string out = text;
	const size_t point = out.find('.');

	while (out.size() > point + 2 && out.back() == '0')
		out.pop_back();

	return out;
}

std::string Colour(const float* parts)
{
	std::string out = "[ ";

	for (int i = 0; i < kColourParts; ++i)
		out += Real(parts[i]) + (i + 1 < kColourParts ? ", " : " ]");

	return out;
}

float Clamped(float value, float least, float most)
{
	return value < least ? least : (value > most ? most : value);
}

void HoverTip(const char* text)
{
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", text);
}

}

StageFieldsWindow::StageFieldsWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void StageFieldsWindow::Show(int id, int slot, const std::string& name)
{
	m_id = id;
	m_slot = slot;
	m_title = "Advanced: " + name + "###stagefields";
	m_focus = true;

	Refresh();
	Open();
}

void StageFieldsWindow::Refresh()
{
	m_values.assign(kFieldCount, std::string());
	m_forced.assign(kFieldCount, false);

	for (int i = 0; i < kFieldCount; ++i)
	{
		m_values[i] = StageImport::FieldOf(m_id, kFields[i].key);
		m_forced[i] = StageImport::Forces(m_id, kFields[i].key);
	}

	m_edited = StageImport::EditedFields(m_id);
	m_revision = StageLibrary::Revision();
}

void StageFieldsWindow::BeforeDraw()
{
	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(kWindowWidth), Ui::Scaled(kWindowHeight)),
		ImGuiCond_FirstUseEver);

	if (!m_focus)
		return;

	ImGui::SetNextWindowFocus();
	m_focus = false;
}

void StageFieldsWindow::Draw()
{
	const bool gameOwns = StageLibrary::GameOwns(m_id);
	StageLibrary::Entry entry = {};

	if (!gameOwns && !StageLibrary::Of(m_id, entry))
	{
		Close();
		return;
	}

	if (m_revision != StageLibrary::Revision() && !ImGui::IsAnyItemActive())
		Refresh();

	m_slot = gameOwns ? m_id : entry.slot;

	UiText::Muted("The camera is saved with the Size column, the rest in %sbg%03d\\stage.txt. A "
		"change shows the next time the stage loads.", gameOwns ? "own\\" : "", m_id);

	ImGui::PushItemWidth(Ui::Scaled(kFieldWidth));

	DrawCamera();

	const char* group = nullptr;

	for (int i = 0; i < kFieldCount; ++i)
	{
		if (group == nullptr || strcmp(group, kFields[i].group) != 0)
		{
			group = kFields[i].group;

			if (GroupHeader(group, GroupEdited(group)))
				ResetGroup(group);
		}

		DrawField(i);
	}

	ImGui::PopItemWidth();

	DrawResetAll();

	if (!gameOwns)
		DrawExport();
}

bool StageFieldsWindow::GroupHeader(const char* group, bool edited)
{
	ImGui::PushID(group);
	ImGui::Spacing();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(group);

	const float button = ImGui::CalcTextSize("Reset").x + ImGui::GetStyle().FramePadding.x * 2.0f;

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - button);
	ImGui::BeginDisabled(!edited);

	const bool pressed = ImGui::SmallButton("Reset");

	ImGui::EndDisabled();
	HoverTip("Puts back the values this stage came with for this part.");

	ImGui::Separator();
	ImGui::PopID();

	return pressed;
}

bool StageFieldsWindow::GroupEdited(const char* group) const
{
	for (const Field& field : kFields)
	{
		if (strcmp(field.group, group) != 0)
			continue;

		if (std::find(m_edited.begin(), m_edited.end(), field.key) != m_edited.end())
			return true;
	}

	return false;
}

void StageFieldsWindow::ResetGroup(const char* group)
{
	std::vector<std::string> keys;

	for (const Field& field : kFields)
	{
		if (strcmp(field.group, group) == 0)
			keys.push_back(field.key);
	}

	StageImport::ResetFields(m_id, keys);
	Refresh();
}

void StageFieldsWindow::DrawCamera()
{
	const bool placed = m_slot >= 0 && StagePlacement::Edited(m_slot);

	if (GroupHeader("Camera", placed))
		StagePlacement::Forget(m_slot);

	StagePlacement::Place place = {};

	if (m_slot < 0 || !StagePlacement::Of(m_slot, place))
	{
		UiText::Muted("Enable the stage in the game to change its camera.");
		return;
	}

	bool changed = ImGui::DragFloat3("Scale##fieldscale", place.scale, kScaleSpeed, 0.0f, kMostScale,
		"%.3f");
	HoverTip("How big the scenery is. The Size column changes all three at once.");

	changed |= ImGui::DragFloat3("Position##fieldmove", place.position, kMoveSpeed, -kMostMove,
		kMostMove, "%.2f");
	HoverTip("Moves the scenery. The fighters stay where they are.");

	changed |= ImGui::DragFloat("FOV##fieldfov", &place.fov, kFovSpeed, StagePlacement::kLeastFov,
		StagePlacement::kMostFov, "%.1f");
	HoverTip("Field of view in degrees.");

	changed |= ImGui::DragFloat("Vanishing point##fieldhorizon", &place.horizon, kHorizonSpeed,
		-kMostHorizon, kMostHorizon, "%.4f");
	HoverTip("Slides the view up or down without tilting it.");

	changed |= ImGui::DragFloat("View rotation X##fieldtilt", &place.tilt, kAngleSpeed, -kMostTilt,
		kMostTilt, "%.1f");
	HoverTip("Tips the scenery toward or away from the camera, in degrees.");

	changed |= ImGui::DragFloat("View rotation Y##fieldturn", &place.turn, kAngleSpeed, -kMostTurn,
		kMostTurn, "%.1f");
	HoverTip("Turns the scenery left or right around the fight, in degrees.");

	if (changed)
		StagePlacement::Set(m_slot, place);
}

void StageFieldsWindow::DrawField(int index)
{
	const Field& field = kFields[index];
	std::string& text = m_values[index];
	const bool forced = m_forced[index];

	ImGui::PushID(index);
	ImGui::BeginDisabled(forced);

	float parts[kColourParts] = {};
	const int read = Numbers(text, parts, field.kind == Kind::Colour ? kColourParts : 1);

	switch (field.kind)
	{
	case Kind::Toggle:
	{
		bool on = read > 0 && parts[0] != 0.0f;

		if (ImGui::Checkbox(field.label, &on))
			Commit(index, on ? "1" : "0");

		break;
	}
	case Kind::Whole:
	{
		int value = read > 0 ? static_cast<int>(parts[0]) : static_cast<int>(field.least);

		if (ImGui::DragInt(field.label, &value, field.speed, static_cast<int>(field.least),
			static_cast<int>(field.most)))
		{
			text = std::to_string(value);
		}

		if (ImGui::IsItemDeactivatedAfterEdit())
			Commit(index, text);

		break;
	}
	case Kind::Real:
	{
		float value = read > 0 ? parts[0] : field.least;

		if (ImGui::DragFloat(field.label, &value, field.speed, field.least, field.most, "%.3f"))
			text = Real(Clamped(value, field.least, field.most));

		if (ImGui::IsItemDeactivatedAfterEdit())
			Commit(index, text);

		break;
	}
	case Kind::Colour:
	{
		if (ImGui::ColorEdit4(field.label, parts, ImGuiColorEditFlags_Float))
			text = Colour(parts);

		if (ImGui::IsItemDeactivatedAfterEdit())
			Commit(index, text);

		break;
	}
	}

	ImGui::EndDisabled();

	HoverTip(forced ? "The mod always sets this for this game's stages." : field.help);

	ImGui::PopID();
}

void StageFieldsWindow::Commit(int index, const std::string& value)
{
	StageImport::SetField(m_id, kFields[index].key, value);
	Refresh();
}

void StageFieldsWindow::DrawResetAll()
{
	ImGui::Spacing();
	ImGui::Separator();

	const bool placed = m_slot >= 0 && StagePlacement::Edited(m_slot);

	ImGui::BeginDisabled(m_edited.empty() && !placed);

	if (ImGui::Button("Reset all"))
	{
		StageImport::ResetFields(m_id, m_edited);

		if (placed)
			StagePlacement::Forget(m_slot);

		Refresh();
	}

	ImGui::EndDisabled();
	HoverTip("Puts back every value this stage came with.");

	UiText::Muted("%s", StageImport::StatusText());
}

void StageFieldsWindow::DrawExport()
{
	ImGui::Spacing();
	ImGui::SeparatorText("Export");

	UiText::Muted("Puts this stage into BBTAG or BBCF in place of one of their stages. Neither game "
		"can take a new stage, so one has to make room. The original is backed up first.");

	const bool busy = StageExport::IsBusy();

	if (!m_targetsLoaded || (m_exportBusy && !busy))
		RefreshTargets();

	m_exportBusy = busy;

	DrawGamePicker();
	DrawTargetPicker();
	DrawInstallButtons();

	if (!m_installStatus.empty())
		UiText::Muted("%s", m_installStatus.c_str());

	UiText::Muted("%s", StageExport::StatusText().c_str());
}

void StageFieldsWindow::DrawGamePicker()
{
	std::string picked;

	if (m_folderDialog.TakeResult(picked) && !picked.empty())
	{
		m_installStatus = StageInstall::ChooseGameFolder(m_exportGame, picked)
			? std::string() : std::string("That folder has no ") + LabelOf(m_exportGame) + " in it.";
		RefreshTargets();
	}

	ImGui::SetNextItemWidth(Ui::Scaled(kFieldWidth));

	if (ImGui::BeginCombo("Game", LabelOf(m_exportGame)))
	{
		for (const ExportGame& one : kExportGames)
		{
			if (ImGui::Selectable(one.label, one.game == m_exportGame) && one.game != m_exportGame)
			{
				m_exportGame = one.game;
				m_installStatus.clear();
				RefreshTargets();
			}
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(m_folderDialog.IsRunning());

	if (ImGui::Button("Choose folder"))
		m_folderDialog.BeginFolder("Pick the game folder");

	ImGui::EndDisabled();

	if (m_gameFolder.empty())
		UiText::Muted("%s was not found. Choose its folder.", LabelOf(m_exportGame));
	else
		UiText::Muted("%s", m_gameFolder.c_str());
}

void StageFieldsWindow::DrawTargetPicker()
{
	if (m_targets.empty())
	{
		if (!m_gameFolder.empty())
			UiText::Muted("No stage files were found in its data\\bg folder.");

		return;
	}

	const char* current = ChosenTarget() == nullptr ? "" : m_targetLabels[m_target].c_str();

	ImGui::SetNextItemWidth(Ui::Scaled(kFieldWidth));

	if (!ImGui::BeginCombo("Replace", current))
		return;

	for (int i = 0; i < static_cast<int>(m_targets.size()); ++i)
	{
		ImGui::PushID(i);

		if (ImGui::Selectable(m_targetLabels[i].c_str(), i == m_target))
			m_target = i;

		ImGui::PopID();
	}

	ImGui::EndCombo();
}

void StageFieldsWindow::DrawInstallButtons()
{
	const StageInstall::Target* chosen = ChosenTarget();
	const bool busy = StageExport::IsBusy();

	ImGui::BeginDisabled(busy || chosen == nullptr);

	if (ImGui::Button("Install in game") && chosen != nullptr)
	{
		m_installStatus.clear();
		StageExport::Install(m_id, m_exportGame, *chosen);
	}

	ImGui::EndDisabled();
	HoverTip("Backs up the stage it replaces the first time, then writes this one over it.");

	ImGui::SameLine();
	ImGui::BeginDisabled(busy || chosen == nullptr || !m_targetBackups[m_target]);

	if (ImGui::Button("Restore original") && chosen != nullptr)
	{
		StageInstall::Restore(m_exportGame, *chosen, m_installStatus);
		RefreshTargets();
	}

	ImGui::EndDisabled();
	HoverTip("Puts the backed up stage back.");

	ImGui::SameLine();
	ImGui::BeginDisabled(busy);

	if (ImGui::Button("Export files only"))
		StageExport::Start(m_id);

	ImGui::EndDisabled();
	HoverTip("Writes the .MUA, .mmot, .evb, textures and both games' .pac files into UNI2-IM\\Export.");

	const std::string folder = StageExport::Folder();

	if (folder.empty())
		return;

	ImGui::SameLine();

	if (ImGui::Button("Open folder"))
		ShellOpen::Open(folder);
}

void StageFieldsWindow::RefreshTargets()
{
	m_targetsLoaded = true;
	m_gameFolder = StageInstall::GameFolder(m_exportGame);
	m_targets = StageInstall::Targets(m_exportGame);
	const int last = static_cast<int>(m_targets.size()) - 1;
	m_target = m_target > last ? (last < 0 ? 0 : last) : m_target;
	m_targetLabels.clear();
	m_targetBackups.clear();

	for (const StageInstall::Target& target : m_targets)
	{
		m_targetLabels.push_back(TargetLabel(m_exportGame, target));
		m_targetBackups.push_back(StageInstall::HasBackup(m_exportGame, target));
	}
}

const StageInstall::Target* StageFieldsWindow::ChosenTarget() const
{
	if (m_target < 0 || m_target >= static_cast<int>(m_targets.size()))
		return nullptr;

	return &m_targets[m_target];
}
