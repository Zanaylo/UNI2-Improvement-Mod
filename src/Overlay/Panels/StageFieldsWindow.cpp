#include "Overlay/Panels/StageFieldsWindow.h"

#include "Game/Stages/StageFields.h"
#include "Game/Stages/StageImport.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Stages/StagePlacement.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

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
	{ "StageW", "Stage width", "Quality", Kind::Whole, 4096, 16384, 16, "Width of the area the fighters can walk. 4096 is the least the game's own stages use." },
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

void StageFieldsWindow::Open(int id, int slot, const std::string& name)
{
	m_id = id;
	m_slot = slot;
	m_name = name;
	m_open = true;

	Refresh();
	ImGui::SetWindowFocus("###stagefields");
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

	m_edited = StageFields::Edited(m_id);

	m_revision = StageLibrary::Revision();
}

void StageFieldsWindow::Draw()
{
	if (!m_open)
		return;

	StageLibrary::Entry entry = {};

	if (!StageLibrary::Of(m_id, entry))
	{
		m_open = false;
		return;
	}

	if (m_revision != StageLibrary::Revision() && !ImGui::IsAnyItemActive())
		Refresh();

	m_slot = entry.slot;

	ImGui::SetNextWindowSize(ImVec2(Ui::Scaled(kWindowWidth), Ui::Scaled(kWindowHeight)),
		ImGuiCond_FirstUseEver);

	char title[128] = {};
	sprintf_s(title, "Advanced: %.96s###stagefields", m_name.c_str());

	if (!ImGui::Begin(title, &m_open))
	{
		ImGui::End();
		return;
	}

	UiText::Muted("The camera is saved with the Size column, the rest in bg%03d\\stage.txt. A change "
		"shows the next time the stage loads.", m_id);

	ImGui::PushItemWidth(Ui::Scaled(kFieldWidth));

	DrawCamera();

	const char* group = nullptr;

	for (int i = 0; i < kFieldCount; ++i)
	{
		if (group == nullptr || strcmp(group, kFields[i].group) != 0)
		{
			group = kFields[i].group;
			ImGui::SeparatorText(group);
		}

		DrawField(i);
	}

	ImGui::PopItemWidth();

	DrawReset();

	ImGui::End();
}

void StageFieldsWindow::DrawCamera()
{
	ImGui::SeparatorText("Camera");

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

void StageFieldsWindow::DrawReset()
{
	ImGui::Separator();

	const bool placed = m_slot >= 0 && StagePlacement::Edited(m_slot);

	ImGui::BeginDisabled(!m_edited && !placed);

	if (ImGui::Button("Reset all"))
	{
		StageImport::ResetFields(m_id);

		if (placed)
			StagePlacement::Forget(m_slot);

		Refresh();
	}

	ImGui::EndDisabled();
	HoverTip("Puts back every value this stage came with.");

	UiText::Muted("%s", StageImport::StatusText());
}
