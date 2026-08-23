#include "Overlay/Window/ColorCustomizePanel.h"

#include "Core/utils.h"
#include "Game/CharaTables.h"
#include "Game/ColorOverride.h"
#include "Game/ColorPartTable.h"
#include "Overlay/ComboNav.h"
#include "Palette/PaletteFile.h"
#include "Palette/PaletteManager.h"

#include "Overlay/UiScale.h"

#include <imgui.h>

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int kGridColumns = 32;
constexpr float kGridCell = 12.0f;
constexpr float kSampleCell = 16.0f;

constexpr float kPreviewWidth = 240.0f;
constexpr float kPreviewHeight = 300.0f;

constexpr int kHistoryMax = 64;

constexpr ImVec4 kWarning = ImVec4(1.0f, 0.72f, 0.25f, 1.0f);

ImU32 ColourAt(const uint8_t* row, int index)
{
	if (row == nullptr || index < 0 || index >= StockPalettes::kColours)
		return IM_COL32(0, 0, 0, 255);

	const uint8_t* const colour = row + index * 4;
	return IM_COL32(colour[0], colour[1], colour[2], 255);
}

void ColourName(int chara, int colour, char* out, size_t size)
{
	if (colour >= ColorCustomize::kCustomFirst)
	{
		sprintf_s(out, size, "Custom %d", colour - ColorCustomize::kCustomFirst + 1);
		return;
	}

	if (colour < StockPalettes::GetCount(chara))
	{
		sprintf_s(out, size, "Colour %02d", colour + 1);
		return;
	}

	sprintf_s(out, size, "Colour %02d (unused)", colour + 1);
}

void DrawSwatches(const uint8_t* row, const int* indices, int count, float cell)
{
	if (row == nullptr || indices == nullptr || count <= 0)
	{
		ImGui::Dummy(ImVec2(cell, cell));
		return;
	}

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList* const draw = ImGui::GetWindowDrawList();

	for (int i = 0; i < count; ++i)
	{
		const ImVec2 start = ImVec2(origin.x + i * cell, origin.y);
		const ImVec2 corner = ImVec2(start.x + cell, origin.y + cell);

		draw->AddRectFilled(start, corner, ColourAt(row, indices[i]));
	}

	draw->AddRect(origin, ImVec2(origin.x + count * cell, origin.y + cell),
		IM_COL32(20, 20, 24, 255));

	ImGui::Dummy(ImVec2(count * cell, cell));
}

}

void ColorCustomizePanel::Draw()
{
	if (!ColorCustomize::IsAvailable())
	{
		ImGui::TextDisabled("The game has not built its player card yet.");
		return;
	}

	ColorPartTable::Load();

	DrawCharacter();

	if (!StockPalettes::Load(m_chara))
	{
		ImGui::TextDisabled("The game's colour files could not be read.");
		return;
	}

	if (m_pulledChara != m_chara || m_pulledSlot != m_slot)
		Pull();

	DrawPose();
	DrawPreview();
	ImGui::Separator();
	ImGui::Separator();
	DrawSlot();
	ImGui::Separator();
	DrawActions();

	if (m_status[0] == '\0')
		return;

	ImGui::Separator();
	ImGui::TextWrapped("%s", m_status);
}

void ColorCustomizePanel::DrawCharacter()
{
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Character");
	ImGui::SameLine(ImGui::GetFontSize() * 8.0f);

	Ui::SetItemWidth(260.0f);

	if (ImGui::BeginCombo("##chara", CharaTables::Name(m_chara)))
	{
		const int count = CharaTables::GetCharaCount();

		for (int chara = 0; chara < count; ++chara)
		{
			const bool selected = chara == m_chara;

			ImGui::PushID(chara);

			if (ImGui::Selectable(CharaTables::Name(chara), selected))
				m_chara = chara;

			ComboNav::KeepSelectedInView(selected);

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	const int steps = ComboNav::WheelSteps();
	if (steps == 0)
		return;

	const int target = m_chara + steps;
	if (target >= 0 && target < CharaTables::GetCharaCount())
		m_chara = target;
}

void ColorCustomizePanel::DrawPose()
{
	const int count = m_preview.GetFrameCount(m_chara);

	if (m_frame >= count)
		m_frame = 0;

	if (count <= 1)
		return;

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Pose");
	ImGui::SameLine(ImGui::GetFontSize() * 8.0f);

	char label[24] = {};
	sprintf_s(label, "Pose %d", m_frame + 1);

	Ui::SetItemWidth(260.0f);

	if (ImGui::BeginCombo("##pose", label))
	{
		for (int frame = 0; frame < count; ++frame)
		{
			char entry[24] = {};
			sprintf_s(entry, "Pose %d", frame + 1);

			const bool selected = frame == m_frame;

			ImGui::PushID(frame);

			if (ImGui::Selectable(entry, selected))
				m_frame = frame;

			ComboNav::KeepSelectedInView(selected);

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	const int steps = ComboNav::WheelSteps();
	if (steps == 0)
		return;

	const int target = m_frame + steps;
	if (target >= 0 && target < count)
		m_frame = target;
}

void ColorCustomizePanel::Compose(uint8_t* out) const
{
	const uint8_t* const base = StockPalettes::GetRow(m_chara, m_edit.values[0]);
	if (base == nullptr)
	{
		memset(out, 0, StockPalettes::kColours * 4);
		return;
	}

	memcpy(out, base, StockPalettes::kColours * 4);

	for (int part = 1; part < ColorCustomize::kPartCount; ++part)
	{
		if (m_edit.values[part] == m_edit.values[0])
			continue;

		const uint8_t* const row = StockPalettes::GetRow(m_chara, m_edit.values[part]);
		const int* const indices = ColorPartTable::GetIndices(m_chara, part);
		const int count = ColorPartTable::GetIndexCount(m_chara, part);

		if (row == nullptr || indices == nullptr)
			continue;

		for (int i = 0; i < count; ++i)
		{
			const int index = indices[i];
			if (index < 0 || index >= StockPalettes::kColours)
				continue;

			memcpy(out + index * 4, row + index * 4, 4);
		}
	}

	// The picks go on last so they read as "this part is that colour", whatever it was built from.
	for (int part = 0; part < ColorCustomize::kPartCount; ++part)
	{
		uint8_t rgb[3] = {};
		if (!ColorOverride::Get(m_chara, m_slot, part, rgb))
			continue;

		const uint8_t* const reference = ReferenceShade(out, part);
		if (reference == nullptr)
			continue;

		if (part > 0)
		{
			ColorOverride::Retint(out, ColorPartTable::GetIndices(m_chara, part),
				ColorPartTable::GetIndexCount(m_chara, part), reference, rgb);
			continue;
		}

		// The base owns whatever no part claimed, so recolour the complement.
		int owned[StockPalettes::kColours] = {};

		for (int other = 1; other < ColorCustomize::kPartCount; ++other)
		{
			const int* const list = ColorPartTable::GetIndices(m_chara, other);
			const int size = ColorPartTable::GetIndexCount(m_chara, other);

			for (int i = 0; list != nullptr && i < size; ++i)
			{
				if (list[i] >= 0 && list[i] < StockPalettes::kColours)
					owned[list[i]] = 1;
			}
		}

		int rest[StockPalettes::kColours] = {};
		int count = 0;

		for (int index = 1; index < StockPalettes::kColours; ++index)
		{
			if (!owned[index])
				rest[count++] = index;
		}

		ColorOverride::Retint(out, rest, count, reference, rgb);
	}
}

// The swatch the game itself shows for a part, which is what a pick is understood to replace.
const uint8_t* ColorCustomizePanel::ReferenceShade(const uint8_t* palette, int part) const
{
	const int* const samples = ColorPartTable::GetSamples(m_chara, part);
	const int count = ColorPartTable::GetSampleCount(m_chara, part);

	if (samples == nullptr || count <= 0)
		return nullptr;

	const uint8_t* best = nullptr;
	int brightest = -1;

	for (int i = 0; i < count; ++i)
	{
		const int index = samples[i];
		if (index < 0 || index >= StockPalettes::kColours)
			continue;

		const uint8_t* const entry = palette + index * 4;
		const int lit = entry[0] * 299 + entry[1] * 587 + entry[2] * 114;

		if (lit > brightest)
		{
			brightest = lit;
			best = entry;
		}
	}

	return best;
}

void ColorCustomizePanel::DrawPreview()
{
	uint8_t composed[StockPalettes::kColours * 4] = {};
	Compose(composed);

	const bool drawn = m_preview.Draw(m_chara, m_frame, composed, Ui::Scaled(kPreviewWidth),
		Ui::Scaled(kPreviewHeight));

	if (drawn)
		ImGui::SameLine();

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList* const draw = ImGui::GetWindowDrawList();

	const int rows = StockPalettes::kColours / kGridColumns;
	const float cell = Ui::Scaled(kGridCell);

	for (int index = 0; index < StockPalettes::kColours; ++index)
	{
		const ImVec2 start = ImVec2(origin.x + (index % kGridColumns) * cell,
			origin.y + (index / kGridColumns) * cell);
		const ImVec2 corner = ImVec2(start.x + cell, start.y + cell);

		draw->AddRectFilled(start, corner, ColourAt(composed, index));
	}

	const int* const highlight = m_hovered > 0
		? ColorPartTable::GetIndices(m_chara, m_hovered) : nullptr;
	const int highlighted = m_hovered > 0 ? ColorPartTable::GetIndexCount(m_chara, m_hovered) : 0;

	for (int i = 0; highlight != nullptr && i < highlighted; ++i)
	{
		const int index = highlight[i];
		if (index < 0 || index >= StockPalettes::kColours)
			continue;

		const ImVec2 start = ImVec2(origin.x + (index % kGridColumns) * cell,
			origin.y + (index / kGridColumns) * cell);
		const ImVec2 corner = ImVec2(start.x + cell, start.y + cell);

		draw->AddRect(start, corner, IM_COL32(255, 255, 255, 255));
	}

	draw->AddRect(origin, ImVec2(origin.x + kGridColumns * cell, origin.y + rows * cell),
		IM_COL32(120, 120, 130, 255));

	ImGui::Dummy(ImVec2(kGridColumns * cell, rows * cell));

	if (!drawn)
		ImGui::TextDisabled("The character's poses could not be read out of the game's archive.");

	if (!ColorPartTable::IsLoaded())
		ImGui::TextDisabled("The game's part lists could not be read - the parts are not shown.");
}
void ColorCustomizePanel::DrawSlot()
{
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Custom slot");
	ImGui::SameLine(ImGui::GetFontSize() * 8.0f);

	char label[32] = {};
	sprintf_s(label, "Custom %d", m_slot + 1);

	Ui::SetItemWidth(260.0f);

	if (ImGui::BeginCombo("##slot", label))
	{
		for (int slot = 0; slot < ColorCustomize::kSlotCount; ++slot)
		{
			char entry[32] = {};
			sprintf_s(entry, "Custom %d", slot + 1);

			const bool selected = slot == m_slot;

			ImGui::PushID(slot);

			if (ImGui::Selectable(entry, selected))
				m_slot = slot;

			ComboNav::KeepSelectedInView(selected);

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	StepSlot(ComboNav::WheelSteps());

	m_hovered = -1;

	for (int part = 0; part < ColorCustomize::kPartCount; ++part)
		DrawPart(part);
}

void ColorCustomizePanel::DrawPart(int part)
{
	ImGui::PushID(part);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(ColorCustomize::PartName(part));

	if (ImGui::IsItemHovered())
		m_hovered = part;

	ImGui::SameLine(ImGui::GetFontSize() * 8.0f);

	char label[48] = {};
	ColourName(m_chara, m_edit.values[part], label, sizeof(label));

	Ui::SetItemWidth(260.0f);

	if (ImGui::BeginCombo("##colour", label))
	{
		const int count = PaletteCount();

		for (int colour = 0; colour < count; ++colour)
		{
			char entry[48] = {};
			ColourName(m_chara, colour, entry, sizeof(entry));

			const bool selected = colour == m_edit.values[part];

			ImGui::PushID(colour);

			if (ImGui::Selectable(entry, selected))
			{
				Record();
				m_edit.values[part] = colour;
			}

			ComboNav::KeepSelectedInView(selected);

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	if (ImGui::IsItemHovered())
		m_hovered = part;

	StepColour(part, ComboNav::WheelSteps());

	const int* const samples = ColorPartTable::GetSamples(m_chara, part);
	const int sampleCount = ColorPartTable::GetSampleCount(m_chara, part);

	if (samples != nullptr && sampleCount > 0)
	{
		ImGui::SameLine();

		uint8_t composed[StockPalettes::kColours * 4] = {};
		Compose(composed);

		DrawSwatches(composed, samples, sampleCount, Ui::Scaled(kSampleCell));
	}

	ImGui::SameLine();
	DrawPick(part);

	ImGui::PopID();
}


void ColorCustomizePanel::DrawPick(int part)
{
	uint8_t rgb[3] = {};
	const bool picked = ColorOverride::Get(m_chara, m_slot, part, rgb);

	if (!picked)
	{
		uint8_t composed[StockPalettes::kColours * 4] = {};
		Compose(composed);

		const uint8_t* const reference = ReferenceShade(composed, part);

		if (reference != nullptr)
			memcpy(rgb, reference, 3);
	}

	float colour[3] = { rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f };

	if (ImGui::ColorEdit3("##pick", colour,
		ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
	{
		const uint8_t chosen[3] = {
			static_cast<uint8_t>(colour[0] * 255.0f + 0.5f),
			static_cast<uint8_t>(colour[1] * 255.0f + 0.5f),
			static_cast<uint8_t>(colour[2] * 255.0f + 0.5f),
		};

		Record();
		ColorOverride::Set(m_chara, m_slot, part, chosen);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Pick any colour for this part. It replaces the swatch the game shows "
			"for it and carries the rest of its shades with it.\n\n"
			"Everything the mod draws uses it, here and in a match once exported. The slot's own "
			"byte still carries the stock colour, which is all a player without the mod can be "
			"sent.");
	}

	if (!picked)
		return;

	ImGui::SameLine();

	if (ImGui::SmallButton("x"))
	{
		Record();
		ColorOverride::Clear(m_chara, m_slot, part);
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Back to the stock colour.");
}

void ColorCustomizePanel::DrawActions()
{
	const bool dirty = IsDirty();

	ImGui::BeginDisabled(!dirty);

	if (ImGui::Button("Save"))
		Save();

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Writes the slot into the card block and tells the game its data "
			"changed. Nothing above this point touches the game until you press it.");
	}

	ImGui::SameLine();

	ImGui::BeginDisabled(m_history.empty());

	if (ImGui::Button("Undo"))
		Undo();

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Steps back through the changes made since this slot was opened.");

	ImGui::SameLine();

	if (ImGui::Button("Reset slot"))
	{
		int defaults[ColorCustomize::kPartCount] = {};

		if (!ColorPartTable::GetDefault(m_chara, m_slot, defaults))
		{
			SetStatus("The game's default for this slot could not be read.");
		}
		else
		{
			Record();
			memcpy(m_edit.values, defaults, sizeof(m_edit.values));
		}
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Loads the colours the game ships this slot with. Still needs Save.");

	ImGui::SameLine();

	const int colours = PaletteCount();
	const int locked = ColorCustomize::CountLocked(m_chara, colours);

	if (locked == 0)
	{
		ImGui::BeginDisabled();
		ImGui::Button("All colours unlocked");
		ImGui::EndDisabled();
	}
	else
	{
		char label[64] = {};
		sprintf_s(label, "Unlock %d colour%s", locked, locked == 1 ? "" : "s");

		if (ImGui::Button(label))
		{
			for (int colour = 0; colour < colours; ++colour)
				ColorCustomize::Unlock(m_chara, colour);

			SetStatus("Unlocked. The game keeps this the same way it keeps the ones you earn.");
		}
	}

	DrawExport();

	if (!dirty)
		return;

	ImGui::TextColored(kWarning, "Unsaved - changing character or slot throws this away.");
}


void ColorCustomizePanel::DrawExport()
{
	const bool picks = ColorOverride::AnyInSlot(m_chara, m_slot);

	ImGui::BeginDisabled(!picks);

	const bool save = ImGui::Button("Export as palette");

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Writes the composed colours to this character's palette folder as a "
			".pal, which the mod wears in a match. Without a "
			"pick there is nothing to export that the slot does not already carry.");
	}

	if (!save)
		return;

	uint8_t composed[StockPalettes::kColours * 4] = {};
	Compose(composed);

	PaletteFile::Info info = {};
	sprintf_s(info.name, "%s custom %d", CharaTables::Name(m_chara), m_slot + 1);
	strncpy_s(info.creator, "Custom Color", _TRUNCATE);
	strncpy_s(info.description, "Built from the game's own colour customiser.", _TRUNCATE);

	const std::string folder = GetModPalettePath(PaletteManager::GetCharaName(m_chara));
	CreateDirectoryA(folder.c_str(), nullptr);

	const std::string path = folder + "\\" + info.name + PaletteFile::kExtension;

	if (!PaletteFile::Save(path, composed, info))
	{
		SetStatus("The palette could not be written.");
		return;
	}

	PaletteManager::Refresh();
	SetStatus("Exported to this character's palette folder.");
}

void ColorCustomizePanel::Pull()
{
	const bool lost = m_pulledChara >= 0 && IsDirty();

	m_pulledChara = m_chara;
	m_pulledSlot = m_slot;
	m_history.clear();

	m_edit = Edit{};

	if (!ColorCustomize::GetSlot(m_chara, m_slot, m_edit.values)
		|| !ColorCustomize::GetEquipped(m_chara, m_edit.equipped))
	{
		m_edit = Edit{};
		m_saved = m_edit;
		SetStatus("The slot could not be read.");
		return;
	}

	m_saved = m_edit;

	SetStatus(lost ? "Unsaved changes were dropped." : "");
}

void ColorCustomizePanel::Save()
{
	if (!ColorCustomize::SetSlot(m_chara, m_slot, m_edit.values))
	{
		SetStatus("The slot could not be written.");
		return;
	}

	if (!ColorCustomize::SetEquipped(m_chara, m_edit.equipped))
	{
		SetStatus("The slot was written but the worn colour was not.");
		return;
	}

	m_saved = m_edit;
	m_history.clear();

	SetStatus("Saved. The game writes it to SYS-DATA on its own next save.");
}

void ColorCustomizePanel::Undo()
{
	if (m_history.empty())
		return;

	m_edit = m_history.back();
	m_history.pop_back();

	SetStatus("");
}

void ColorCustomizePanel::Record()
{
	m_history.push_back(m_edit);

	if (m_history.size() > kHistoryMax)
		m_history.erase(m_history.begin());
}

bool ColorCustomizePanel::IsDirty() const
{
	return memcmp(&m_edit, &m_saved, sizeof(m_edit)) != 0;
}

void ColorCustomizePanel::StepColour(int part, int steps)
{
	if (steps == 0)
		return;

	const int target = m_edit.values[part] + steps;
	if (target < 0 || target >= PaletteCount())
		return;

	Record();
	m_edit.values[part] = target;
}

void ColorCustomizePanel::StepSlot(int steps)
{
	if (steps == 0)
		return;

	const int target = m_slot + steps;
	if (target < 0 || target >= ColorCustomize::kSlotCount)
		return;

	m_slot = target;
}

void ColorCustomizePanel::SetStatus(const char* text)
{
	strncpy_s(m_status, text, _TRUNCATE);
}

int ColorCustomizePanel::PaletteCount() const
{
	const int count = StockPalettes::GetCount(m_chara);
	return count > 0 ? count : 1;
}
