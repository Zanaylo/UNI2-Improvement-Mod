#include "Overlay/Window/PaletteWindow.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/utils.h"
#include "Game/CharaTables.h"
#include "Game/ColorPartTable.h"
#include "Game/EffectTable.h"
#include "Game/PartColourTable.h"
#include "Game/GameTables.h"
#include "Game/StockPalettes.h"
#include "Overlay/ComboNav.h"
#include "Network/PaletteShare.h"
#include "Palette/EffectOwner.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteChoice.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteFile.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PalettePaint.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kDefaultPalette = "Default";

constexpr float kSwatch = 22.0f;
constexpr int kPerRow = 14;
constexpr int kFlashFrames = 90;

constexpr const char* kPartNames[LivePalette::kParts] = {
	"Base", "Part 1", "Part 2", "Part 3", "Part 4", "Part 5",
};

int Luminance(const uint8_t* rgb)
{
	return (rgb[0] * 299 + rgb[1] * 587 + rgb[2] * 114) / 1000;
}

bool IsUsableName(const char* name)
{
	if (name == nullptr || name[0] == '\0' || name[0] == ' ' || name[0] == '.')
		return false;

	for (const char* at = name; *at != '\0'; ++at)
	{
		if (strchr("\\/:*?\"<>|", *at) != nullptr)
			return false;
	}

	return true;
}

void StripExtension(char* name)
{
	const size_t length = strlen(name);
	const size_t suffix = strlen(PaletteFile::kExtension);

	if (length > suffix && _stricmp(name + length - suffix, PaletteFile::kExtension) == 0)
		name[length - suffix] = 0;
}

bool PickColour(const char* id, const uint8_t* rgb, uint8_t* out)
{
	float picker[3] = { rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f };

	if (!ImGui::ColorEdit3(id, picker, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
		return false;

	for (int c = 0; c < 3; ++c)
		out[c] = static_cast<uint8_t>(picker[c] * 255.0f + 0.5f);

	return true;
}

}

PaletteWindow::PaletteWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void PaletteWindow::BeforeDraw()
{
	ImGui::SetNextWindowSizeConstraints(ImVec2(440.0f, 460.0f), ImVec2(FLT_MAX, FLT_MAX));
	ImGui::SetNextWindowSize(ImVec2(480.0f, 760.0f), ImGuiCond_FirstUseEver);
}

void PaletteWindow::Draw()
{
	if (ImGui::BeginTabBar("##players"))
	{
		for (int player = 0; player < 2; ++player)
		{
			const int chara = PaletteMemory::GetCharaNumber(player);

			char label[72] = {};
			sprintf_s(label, "P%d %s##%d", player + 1,
				chara >= 0 ? CharaTables::Name(chara) : "-", player);

			if (!ImGui::BeginTabItem(label))
				continue;

			ImGui::PushID(player);
			DrawPlayer(player);
			ImGui::PopID();

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	RunFlash();
}

void PaletteWindow::DrawPlayer(int player)
{
	const int chara = PaletteMemory::GetCharaNumber(player);

	if (chara < 0)
	{
		ImGui::TextDisabled("nobody in this slot yet");
		return;
	}

	if (chara != m_chara[player] || PaletteChoice::GetGeneration(player) != m_generation[player])
		Adopt(player, chara);

	if (!StockPalettes::Load(chara))
	{
		ImGui::TextDisabled("this character's colour files could not be read");
		return;
	}

	ColorPartTable::Load();

	PullBaseline(player, false);
	Refresh(player);

	const bool mine = PaletteControl::CanEdit(player);

	if (!mine)
	{

		const uint8_t* const theirs = PalettePaint::GetRemote(player);

		if (theirs != nullptr)
			memcpy(m_composed[player], theirs, sizeof(m_composed[player]));

		ImGui::TextWrapped("%s", PaletteControl::WhyNot(player));

		const char* const name = PaletteShare::GetRemoteName(player);

		if (theirs == nullptr)
		{
			ImGui::TextDisabled("%s", PaletteControl::CanWear(player)
				? "nothing has arrived from them yet"
				: "their colours are switched off in the options");
		}
		else if (name[0] != 0)
		{
			ImGui::TextDisabled("wearing '%s', which they chose", name);
		}
		else
		{
			ImGui::TextDisabled("wearing the colours they picked in the game");
		}

		ImGui::Separator();
	}

	ImGui::BeginDisabled(!mine);

	if (!ImGui::BeginTabBar("##what"))
	{
		ImGui::EndDisabled();
		return;
	}

	if (ImGui::BeginTabItem("Character"))
	{
		DrawParts(player);

		DrawSwatches(player, chara);

		DrawPicker(player);

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Effects"))
	{
		DrawEffects(player);
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();

	ImGui::Separator();
	DrawFiles(player);

	ImGui::EndDisabled();
}

void PaletteWindow::DrawSwatches(int player, int chara)
{
	ImGui::BeginChild("swatches", ImVec2(0.0f, 200.0f), ImGuiChildFlags_Borders);

	const int parts = GameTables::GetPartCount(chara);

	if (g_modVals.paletteGroupByPart && parts > 0)
		DrawGroupedSwatches(player, chara, parts);
	else
		DrawFlatSwatches(player);

	ImGui::EndChild();
}

void PaletteWindow::DrawGroupedSwatches(int player, int chara, int parts)
{
	bool covered[LivePalette::kColours] = {};

	for (int part = 0; part < parts; ++part)
	{
		const char* name = nullptr;
		const unsigned char* entries = nullptr;
		int count = 0;

		if (!GameTables::GetPart(chara, part, name, entries, count))
			continue;

		ImGui::PushID(part);

		if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen))
			DrawGrid(player, entries, count);

		for (int i = 0; i < count; ++i)
			covered[entries[i]] = true;

		ImGui::PopID();
	}

	unsigned char rest[LivePalette::kColours] = {};
	int restCount = 0;

	for (int i = 1; i < LivePalette::kColours; ++i)
	{
		if (!covered[i] && !IsJunk(player, i))
			rest[restCount++] = static_cast<unsigned char>(i);
	}

	if (restCount == 0)
		return;

	ImGui::PushID(parts);

	if (ImGui::CollapsingHeader("Everything else"))
		DrawGrid(player, rest, restCount);

	ImGui::PopID();
}

// The rules the first palette system used, less the one for black. Judged on the game's own colours
// rather than the composed ones, so editing an entry cannot make it disappear from under the cursor.
bool PaletteWindow::IsJunk(int player, int entry) const
{
	if (!g_modVals.paletteFilterJunk)
		return false;

	const uint8_t* const colours = m_colours[player].baseline;
	const uint8_t* const colour = colours + entry * 4;

	// Black stays: it is a real colour on plenty of characters, and hiding it cost more than the
	// padding it also matched.
	if (colour[0] == 0 && colour[1] == 255 && colour[2] == 0)
		return true;

	for (int i = 1; i < entry; ++i)
	{
		if (memcmp(colours + i * 4, colour, 3) == 0)
			return true;
	}

	return false;
}

void PaletteWindow::DrawFlatSwatches(int player)
{
	unsigned char flat[LivePalette::kColours] = {};
	int count = 0;

	for (int i = 1; i < LivePalette::kColours; ++i)
	{
		if (!IsJunk(player, i))
			flat[count++] = static_cast<unsigned char>(i);
	}

	if (count == 0)
	{
		ImGui::TextDisabled("every entry looks like padding - untick Filter junk colours");
		return;
	}

	DrawGrid(player, flat, count);
}

void PaletteWindow::DrawParts(int player)
{
	LivePalette::Colours& colours = m_colours[player];

	if (!ImGui::CollapsingHeader("Whole parts"))
		return;

	ImGui::TextWrapped("Moves a part's whole ramp of shades at once, keeping the shading and "
		"moving only the colour underneath.");

	const int count = StockPalettes::GetCount(colours.chara);

	for (int part = 0; part < LivePalette::kParts; ++part)
	{
		ImGui::PushID(part);

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(kPartNames[part]);
		ImGui::SameLine(ImGui::GetFontSize() * 4.5f);

		char label[32] = {};

		if (colours.stock[part] == LivePalette::kAsDrawn)
			sprintf_s(label, part == 0 ? "As drawn" : "Follow base");
		else
			sprintf_s(label, "Colour %02d", colours.stock[part] + 1);

		ImGui::SetNextItemWidth(140.0f);

		if (ImGui::BeginCombo("##stock", label))
		{

			const bool asDrawn = colours.stock[part] == LivePalette::kAsDrawn;

			if (ImGui::Selectable(part == 0 ? "As drawn" : "Follow base", asDrawn) && !asDrawn)
			{
				Record(player);
				colours.stock[part] = LivePalette::kAsDrawn;
				Apply(player);
			}

			ComboNav::KeepSelectedInView(asDrawn);

			for (int i = 0; i < count; ++i)
			{
				char option[32] = {};
				sprintf_s(option, "Colour %02d", i + 1);

				const bool selected = i == colours.stock[part];

				ImGui::PushID(i);

				if (ImGui::Selectable(option, selected) && !selected)
				{
					Record(player);
					colours.stock[part] = i;
					Apply(player);
				}

				ComboNav::KeepSelectedInView(selected);

				ImGui::PopID();
			}

			ImGui::EndCombo();
		}

		const int steps = ComboNav::WheelSteps();

		if (steps != 0)
		{
			const int target = colours.stock[part] + steps;

			if (target >= LivePalette::kAsDrawn && target < count)
			{
				Record(player);
				colours.stock[part] = target;
				Apply(player);
			}
		}

		ImGui::SameLine();

		uint8_t rgb[3] = { 255, 255, 255 };

		if (colours.picked[part])
		{
			memcpy(rgb, colours.pick[part], 3);
		}
		else
		{
			const int* const samples = ColorPartTable::GetSamples(colours.chara, part);
			const int sampleCount = ColorPartTable::GetSampleCount(colours.chara, part);

			int brightest = -1;

			for (int i = 0; samples != nullptr && i < sampleCount; ++i)
			{
				if (samples[i] <= 0 || samples[i] >= LivePalette::kColours)
					continue;

				const uint8_t* const entry = m_composed[player] + samples[i] * 4;

				if (Luminance(entry) > brightest)
				{
					brightest = Luminance(entry);
					memcpy(rgb, entry, 3);
				}
			}
		}

		uint8_t chosen[3] = {};

		if (PickColour("##partpick", rgb, chosen))
		{
			if (ImGui::IsItemActivated() || !ImGui::IsItemActive())
				Record(player);

			memcpy(colours.pick[part], chosen, 3);
			colours.picked[part] = true;

			Apply(player);
		}

		if (colours.picked[part])
		{
			ImGui::SameLine();

			if (ImGui::SmallButton("x"))
			{
				Record(player);
				colours.picked[part] = false;
				Apply(player);
			}

			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Back to the stock colour.");
		}

		ImGui::PopID();
	}
}

void PaletteWindow::DrawGrid(int player, const unsigned char* entries, int count)
{
	const LivePalette::Colours& colours = m_colours[player];

	for (int n = 0; n < count; ++n)
	{
		const int i = entries[n];

		ImGui::PushID(i);

		const uint8_t* const rgb = m_composed[player] + i * 4;
		const ImVec4 colour(rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f, 1.0f);

		const bool selected = i == m_selected[player];
		const bool changed = colours.edited[i];

		if (selected || changed)
		{
			ImGui::PushStyleColor(ImGuiCol_Border, selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
				: ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
		}

		if (ImGui::ColorButton("##swatch", colour,
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
			ImVec2(kSwatch, kSwatch)))
		{
			m_selected[player] = i;
			StartFlash(player);
		}

		if (selected || changed)
		{
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("entry %d%s", i, changed ? " - changed" : "");

		if (((n + 1) % kPerRow) != 0 && n + 1 < count)
			ImGui::SameLine();

		ImGui::PopID();
	}
}

void PaletteWindow::DrawPicker(int player)
{
	LivePalette::Colours& colours = m_colours[player];
	const int selected = m_selected[player];

	ImGui::Text("entry %d%s", selected, colours.edited[selected] ? "  (changed)" : "");

	float picked[3] = {
		m_composed[player][selected * 4 + 0] / 255.0f,
		m_composed[player][selected * 4 + 1] / 255.0f,
		m_composed[player][selected * 4 + 2] / 255.0f,
	};

	ImGui::SetNextItemWidth(170.0f);

	if (ImGui::ColorPicker3("##entry", picked, ImGuiColorEditFlags_NoSidePreview |
		ImGuiColorEditFlags_NoSmallPreview))
	{

		if (ImGui::IsItemActivated() || !ImGui::IsItemActive())
			Record(player);

		for (int c = 0; c < 3; ++c)
			colours.entry[selected][c] = static_cast<uint8_t>(picked[c] * 255.0f + 0.5f);

		colours.edited[selected] = true;
		Apply(player);
	}

	if (ImGui::Button(m_applied[player] ? "Re-apply" : "Apply"))
		Apply(player);

	ImGui::SameLine();

	ImGui::BeginDisabled(m_historyCount[player] == 0);

	const bool undo = ImGui::Button("Undo");

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Takes back the last change. %d to go.", m_historyCount[player]);

	if (undo)
		Undo(player);

	ImGui::SameLine();

	ImGui::BeginDisabled(!m_applied[player]);

	const bool remove = ImGui::Button("Remove");

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Puts the game's own colours back without losing what you have built, and "
			"stops this character being dressed automatically next match.");
	}

	if (remove)
	{
		PalettePaint::Clear(player);
		EffectPaint::Clear(player);

		PaletteChoice::Forget(m_chara[player]);
		PaletteChoice::NoteBare(player);

		m_applied[player] = false;
		m_chosen[player] = -1;
	}

	ImGui::SameLine();

	if (ImGui::Button("Reset"))
	{
		Record(player);
		LivePalette::Reset(m_colours[player], m_chara[player]);

		if (m_applied[player])
			Apply(player);
	}

	ImGui::SameLine();

	if (ImGui::Button("Re-read"))
	{

		PalettePaint::Clear(player);

		m_applied[player] = false;

		PullBaseline(player, true);
		Refresh(player);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Takes the character's colours from the game again, for when the colour "
			"they are wearing has been changed since this was opened. Your edits are kept.");
	}

	if (!m_applied[player])
		ImGui::TextDisabled("not worn");
	else if (PalettePaint::IsPainting(player))
		ImGui::TextDisabled("worn");
	else
		ImGui::TextDisabled("applied - waiting for this character to draw");
}

void PaletteWindow::DrawEffects(int player)
{
	const int chara = m_chara[player];

	unsigned char entries[LivePalette::kColours] = {};
	int count = 0;

	for (int entry = 1; entry < LivePalette::kColours; ++entry)
	{

		if (PartColourTable::IsPartEntry(chara, entry) || EffectTable::CountUsing(chara, entry) > 0)
			entries[count++] = static_cast<unsigned char>(entry);
	}

	if (count == 0)
	{
		ImGui::TextDisabled("this character's effects are not tinted from its palette");
		return;
	}

	ImGui::Text("%d entries, %d seen drawn, %d changed", count,
		EffectPaint::GetObservedCount(player), EffectPaint::GetEditedCount(player));

	ImGui::SameLine();

	if (ImGui::Button("Reset effects"))
		EffectPaint::Clear(player);

	ImGui::BeginChild("effects", ImVec2(0.0f, 150.0f), ImGuiChildFlags_Borders);

	for (int n = 0; n < count; ++n)
	{
		const int entry = entries[n];

		uint8_t rgb[3] = {};
		bool observed = EffectPaint::GetObserved(player, entry, rgb);

		if (!observed)
			observed = EffectPaint::GetAnyObserved(entry, rgb);

		if (!observed)
			memcpy(rgb, m_composed[player] + entry * 4, 3);

		uint8_t edit[3] = {};
		const bool edited = EffectPaint::GetRemoteEntry(player, entry, edit)
			|| EffectPaint::GetEdit(player, entry, edit);

		if (edited)
			memcpy(rgb, edit, 3);

		ImGui::PushID(entry);

		const bool selected = entry == m_effectEntry[player];

		if (selected || edited)
		{
			ImGui::PushStyleColor(ImGuiCol_Border, selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
				: ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
		}

		if (ImGui::ColorButton("##effect",
			ImVec4(rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f, 1.0f),
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
			ImVec2(kSwatch, kSwatch)))
		{
			m_effectEntry[player] = entry;
		}

		if (selected || edited)
		{
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("entry %d - used by %d part%s%s%s%s", entry,
				PartColourTable::GetPartCount(chara, entry),
				PartColourTable::GetPartCount(chara, entry) == 1 ? "" : "s",
				observed ? "" : ", not drawn yet so the palette's own colour stands in",
				edited ? ", changed" : "",
				EffectOwner::Claims(player == 0 ? 1 : 0, entry)
					? "\nthe other character uses this entry too, so an effect neither "
						"palette tells apart is left alone" : "");
		}

		if (((n + 1) % kPerRow) != 0 && n + 1 < count)
			ImGui::SameLine();

		ImGui::PopID();
	}

	ImGui::EndChild();

	const int entry = m_effectEntry[player];

	if (entry < 0)
	{
		ImGui::TextDisabled("pick one to change it");
		return;
	}

	uint8_t rgb[3] = {};

	if (!EffectPaint::GetRemoteEntry(player, entry, rgb)
		&& !EffectPaint::GetEdit(player, entry, rgb)
		&& !EffectPaint::GetObserved(player, entry, rgb))
	{
		memcpy(rgb, m_composed[player] + entry * 4, 3);
	}

	ImGui::Text("entry %d%s", entry, EffectPaint::IsEdited(player, entry) ? "  (changed)" : "");

	float picked[3] = { rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f };

	ImGui::SetNextItemWidth(170.0f);

	if (ImGui::ColorPicker3("##effectpick", picked, ImGuiColorEditFlags_NoSidePreview |
		ImGuiColorEditFlags_NoSmallPreview))
	{
		const uint8_t chosen[3] = {
			static_cast<uint8_t>(picked[0] * 255.0f + 0.5f),
			static_cast<uint8_t>(picked[1] * 255.0f + 0.5f),
			static_cast<uint8_t>(picked[2] * 255.0f + 0.5f),
		};

		EffectPaint::SetEntry(player, entry, chosen);
	}

	if (EffectPaint::IsEdited(player, entry) && ImGui::Button("Back to the game's colour"))
		EffectPaint::ClearEntry(player, entry);
}

void PaletteWindow::LoadCreator()
{
	if (m_creatorLoaded)
		return;

	m_creatorLoaded = true;

	for (int player = 0; player < 2; ++player)
	{
		GetPrivateProfileStringA("Palette", "Creator", "", m_creator[player],
			sizeof(m_creator[player]), Settings::GetIniPath().c_str());
	}
}

void PaletteWindow::DrawFiles(int player)
{
	LoadCreator();

	// A side can stop being editable while its name is half typed - the local side is only known
	// once a match starts, and a disabled field drops ImGui's focus, which hands the rest of the
	// word to the game.
	const bool mine = PaletteControl::CanEdit(player);
	const bool editingName = ImGui::GetActiveID() == ImGui::GetID("##name");

	if (editingName)
		ImGui::EndDisabled();

	ImGui::TextUnformatted("Name");

	ImGui::SetNextItemWidth(170.0f);
	ImGui::InputText("##name", m_name[player], sizeof(m_name[player]));

	if (editingName)
		ImGui::BeginDisabled(!mine);

	ImGui::SameLine();

	const bool nameOk = IsUsableName(m_name[player]);

	ImGui::BeginDisabled(!nameOk);

	const bool save = ImGui::Button("Save");

	ImGui::EndDisabled();

	if (save)
	{
		const bool written = Save(player);

		sprintf_s(m_status[player], written ? "Saved." : "Could not write the file.");
		RefreshFiles(player);

		if (written)
			SelectFile(player, (std::string(m_name[player]) + PaletteFile::kExtension).c_str());
	}

	ImGui::TextUnformatted("Author");

	ImGui::SetNextItemWidth(170.0f);
	ImGui::InputText("##author", m_creator[player], sizeof(m_creator[player]));

	// Remembered as soon as it is typed rather than only when a palette is saved: it is whoever is
	// at the keyboard, not a property of one file.
	if (ImGui::IsItemDeactivatedAfterEdit())
		Settings::SaveString("Palette", "Creator", m_creator[player]);

	ImGui::TextUnformatted("Description");

	ImGui::SetNextItemWidth(340.0f);
	ImGui::InputText("##description", m_description[player], sizeof(m_description[player]));

	ImGui::TextUnformatted("Character Palette");

	const char* const chosen = m_chosen[player] >= 0 && m_chosen[player] < m_fileCount[player]
		? m_files[player][m_chosen[player]].c_str() : kDefaultPalette;

	ImGui::SetNextItemWidth(170.0f);

	if (ImGui::BeginCombo("##load", chosen))
	{
		const bool bare = m_chosen[player] < 0;

		if (ImGui::Selectable(kDefaultPalette, bare))
			Bare(player);

		ComboNav::KeepSelectedInView(bare);

		for (int i = 0; i < m_fileCount[player]; ++i)
		{
			const bool selected = i == m_chosen[player];

			ImGui::PushID(i);

			if (ImGui::Selectable(m_files[player][i].c_str(), selected))
			{
				m_chosen[player] = i;

				sprintf_s(m_status[player], Load(player, m_files[player][i].c_str())
					? "Loaded." : "Could not read the file.");
			}

			ComboNav::KeepSelectedInView(selected);

			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	// The wheel walks the same list the combo shows, so -1 is Default rather than "nothing".
	const int steps = ComboNav::WheelSteps();
	if (steps != 0)
	{
		int target = m_chosen[player] + steps;

		if (target < -1)
			target = -1;
		if (target >= m_fileCount[player])
			target = m_fileCount[player] - 1;

		if (target != m_chosen[player])
		{
			if (target < 0)
			{
				Bare(player);
			}
			else
			{
				m_chosen[player] = target;

				sprintf_s(m_status[player], Load(player, m_files[player][target].c_str())
					? "Loaded." : "Could not read the file.");
			}
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Rescan"))
		RefreshFiles(player);

	if (!nameOk && m_name[player][0] != '\0')
		ImGui::TextDisabled("that name cannot be a filename");
	else if (m_status[player][0] != '\0')
		ImGui::TextDisabled("%s", m_status[player]);
}

void PaletteWindow::Record(int player)
{
	if (m_historyCount[player] >= kUndoDepth)
	{
		for (int i = 1; i < kUndoDepth; ++i)
			m_history[player][i - 1] = m_history[player][i];

		--m_historyCount[player];
	}

	m_history[player][m_historyCount[player]++] = m_colours[player];
}

void PaletteWindow::Undo(int player)
{
	if (m_historyCount[player] == 0)
		return;

	m_colours[player] = m_history[player][--m_historyCount[player]];

	if (m_applied[player])
		Apply(player);
	else
		Refresh(player);
}

// Rescanning must not change what is selected. -1 means Default now, so blanking the selection here
// made a save look like it had put the game's own colours back.
void PaletteWindow::RefreshFiles(int player)
{
	std::string keep;

	if (m_chosen[player] >= 0 && m_chosen[player] < m_fileCount[player])
		keep = m_files[player][m_chosen[player]];

	m_fileCount[player] = 0;
	m_chosen[player] = -1;

	if (m_chara[player] < 0)
		return;

	PaletteLibrary::Rescan(m_chara[player]);

	const int count = PaletteLibrary::GetCount(m_chara[player]);

	for (int i = 0; i < count && m_fileCount[player] < 64; ++i)
		m_files[player][m_fileCount[player]++] = PaletteLibrary::GetName(m_chara[player], i);

	if (!keep.empty())
		SelectFile(player, keep.c_str());
}

void PaletteWindow::SelectFile(int player, const char* file)
{
	m_chosen[player] = -1;

	if (file == nullptr || file[0] == 0)
		return;

	for (int i = 0; i < m_fileCount[player]; ++i)
	{
		if (m_files[player][i] == file)
		{
			m_chosen[player] = i;
			return;
		}
	}
}

// The combo's Default entry. Same thing the Remove button does: the game's own colours back, and
// this character is not dressed automatically next match either.
void PaletteWindow::Bare(int player)
{
	PalettePaint::Clear(player);
	EffectPaint::Clear(player);

	PaletteChoice::Forget(m_chara[player]);
	PaletteChoice::NoteBare(player);

	m_applied[player] = false;
	m_chosen[player] = -1;

	sprintf_s(m_status[player], "The game's own colours.");
}

bool PaletteWindow::Save(int player)
{
	Refresh(player);

	Settings::SaveString("Palette", "Creator", m_creator[player]);

	PaletteFile::Info info = {};
	strncpy_s(info.name, m_name[player], _TRUNCATE);
	strncpy_s(info.creator, m_creator[player], _TRUNCATE);
	strncpy_s(info.description, m_description[player], _TRUNCATE);

	const std::string folder = PaletteLibrary::FolderFor(m_chara[player]);
	CreateDirectoryA(folder.c_str(), nullptr);

	uint8_t effects[EffectPaint::kBlockBytes] = {};
	EffectPaint::GetBlock(player, effects);

	const std::string file = std::string(m_name[player]) + PaletteFile::kExtension;

	if (!PaletteFile::Save(folder + "\\" + file, m_composed[player], info,
		EffectPaint::GetEditedCount(player) > 0 ? effects : nullptr))
	{
		return false;
	}

	PaletteChoice::Remember(m_chara[player], file.c_str());
	PaletteChoice::NoteWorn(player, file.c_str());

	m_applied[player] = true;
	return true;
}

bool PaletteWindow::Load(int player, const char* name)
{
	uint8_t colours[PaletteFile::kBytes] = {};
	uint8_t effects[PaletteFile::kBytes] = {};
	PaletteFile::Info info = {};
	bool hasEffects = false;

	if (!PaletteFile::Load(PaletteLibrary::FolderFor(m_chara[player]) + "\\" + name, colours, info, effects,
		&hasEffects))
	{
		return false;
	}

	strncpy_s(m_creator[player], info.creator, _TRUNCATE);
	strncpy_s(m_description[player], info.description, _TRUNCATE);

	Record(player);
	LivePalette::Reset(m_colours[player], m_chara[player]);

	for (int i = 1; i < LivePalette::kColours; ++i)
	{
		memcpy(m_colours[player].entry[i], colours + i * 4, 3);
		m_colours[player].edited[i] = true;
	}

	EffectPaint::SetBlock(player, hasEffects ? effects : nullptr);

	Apply(player);

	PaletteChoice::Remember(m_chara[player], name);
	PaletteChoice::NoteWorn(player, name);

	strncpy_s(m_name[player], info.name[0] != '\0' ? info.name : name, _TRUNCATE);
	StripExtension(m_name[player]);

	return true;
}

void PaletteWindow::Adopt(int player, int chara)
{
	LivePalette::Reset(m_colours[player], chara);

	m_chara[player] = chara;
	m_generation[player] = PaletteChoice::GetGeneration(player);
	m_applied[player] = false;
	m_selected[player] = 1;
	m_effectEntry[player] = -1;
	m_historyCount[player] = 0;
	m_pulled[player] = false;
	m_status[player][0] = 0;
	m_chosen[player] = -1;

	RefreshFiles(player);

	char worn[64] = {};
	strncpy_s(worn, PaletteChoice::WornFile(player), _TRUNCATE);

	if (worn[0] == 0)
		return;

	SelectFile(player, worn);

	if (Load(player, worn))
	{

		m_historyCount[player] = 0;
		return;
	}

	sprintf_s(m_status[player], "'%s' could not be read", worn);
}

void PaletteWindow::PullBaseline(int player, bool force)
{
	if (m_pulled[player] && !force)
		return;

	uint8_t drawn[LivePalette::kBytes] = {};

	if (!PalettePaint::ReadGameColours(player, drawn))
		return;

	LivePalette::SetBaseline(m_colours[player], drawn);
	m_pulled[player] = true;
}

void PaletteWindow::Refresh(int player)
{
	LivePalette::Compose(m_colours[player], m_composed[player]);
}

void PaletteWindow::Apply(int player)
{
	Refresh(player);

	PalettePaint::Stage(player, m_composed[player]);
	m_applied[player] = true;
}

void PaletteWindow::StartFlash(int player)
{
	if (!g_modVals.paletteFlashEntry || !PaletteControl::CanEdit(player))
		return;

	if (m_flashPlayer >= 0 && m_flashPlayer != player)
	{
		PalettePaint::EndPreview(m_flashPlayer);
		EffectPaint::EndPreview(m_flashPlayer);
	}

	m_flashPlayer = player;
	m_flashFrames = kFlashFrames;
}

void PaletteWindow::RunFlash()
{
	const int player = m_flashPlayer;

	if (player < 0)
		return;

	if (m_flashFrames <= 0 || !g_modVals.paletteFlashEntry || !PaletteControl::CanEdit(player))
	{
		PalettePaint::EndPreview(player);
		EffectPaint::EndPreview(player);

		m_flashPlayer = -1;
		return;
	}

	--m_flashFrames;

	const int keep = m_selected[player];
	const bool on = (m_flashFrames / 8) % 2 == 0;

	const uint8_t lit[3] = {
		static_cast<uint8_t>(on ? 255 : 0),
		static_cast<uint8_t>(on ? 0 : 255),
		255,
	};

	uint8_t dimmed[LivePalette::kBytes] = {};
	memcpy(dimmed, m_composed[player], sizeof(dimmed));

	for (int i = 0; i < LivePalette::kColours; ++i)
	{
		const uint8_t grey = static_cast<uint8_t>(Luminance(dimmed + i * 4) / 2);

		dimmed[i * 4 + 0] = grey;
		dimmed[i * 4 + 1] = grey;
		dimmed[i * 4 + 2] = grey;
	}

	memcpy(dimmed + keep * 4, lit, 3);

	PalettePaint::Preview(player, dimmed);

	const uint8_t dark[3] = { 40, 40, 40 };

	EffectPaint::PreviewObserved(player, dark, keep, lit);
}
