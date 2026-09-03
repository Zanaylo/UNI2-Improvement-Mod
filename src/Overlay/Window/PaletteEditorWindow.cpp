#include "Overlay/UiScale.h"
#include "Overlay/Window/PaletteEditorWindow.h"

#include "Core/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameState.h"
#include "Game/GameTables.h"
#include "Game/OfferedEntries.h"
#include "Game/PartColourTable.h"
#include "Game/UsedEntryTable.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteTexture.h"

#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

PaletteEditorWindow::PaletteEditorWindow(const std::string& title, bool closable,
	ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

int PaletteEditorWindow::Row() const
{
	return PaletteTexture::GetRowForPlayer(m_player);
}

bool PaletteEditorWindow::ActiveTarget(int& outTexture, int& outRow) const
{
	outTexture = PaletteTexture::FindForPlayer(m_player);
	outRow = Row();

	return outTexture >= 0 && outRow >= 0;
}

void PaletteEditorWindow::Pull()
{

	EndFlash();

	int texture = 0;
	int slot = 0;

	if (!ActiveTarget(texture, slot))
	{
		m_pulled = false;
		return;
	}

	m_pulled = PaletteTexture::ReadRowAsRgba(texture, static_cast<unsigned>(slot), m_colors);

	if (!m_pulled)
		return;

	memcpy(m_original, m_colors, sizeof(m_original));

	if (!PaletteTexture::ReadPristineRowAsRgba(texture, static_cast<unsigned>(slot), m_pristine))
		memcpy(m_pristine, m_colors, sizeof(m_pristine));
}

void PaletteEditorWindow::ForgetIfListChanged()
{
	const int generation = PaletteTexture::GetGeneration();
	const int rowGeneration = PaletteTexture::GetRowGeneration();

	if (generation == m_generation && rowGeneration == m_rowGeneration)
		return;

	m_generation = generation;
	m_rowGeneration = rowGeneration;

	EndFlash();

	m_pulled = false;
	m_status[0] = '\0';

	if (!m_editEffect)
		m_selected = 0;
}

void PaletteEditorWindow::BeforeDraw()
{

	ImGui::SetNextWindowSize(Ui::Scaled(680.0f, 640.0f), ImGuiCond_FirstUseEver);
}

bool PaletteEditorWindow::IsUsed(int chara, int entry) const
{
	const uint8_t* const color = m_pristine + entry * 4;

	if (color[0] == 0 && color[1] == 255 && color[2] == 0)
		return false;

	if (UsedEntryTable::IsUsed(chara, entry))
		return true;

	return PartColourTable::IsPartEntry(chara, entry);
}

void PaletteEditorWindow::DrawSwatches(const unsigned char* entries, int count)
{
	for (int n = 0; n < count; ++n)
	{
		const int i = entries[n];

		ImGui::PushID(i);

		const ImVec4 color(m_colors[i * 4 + 0] / 255.0f, m_colors[i * 4 + 1] / 255.0f,
			m_colors[i * 4 + 2] / 255.0f, 1.0f);

		const bool selected = i == m_selected;
		const bool changed = memcmp(m_colors + i * 4, m_original + i * 4, 3) != 0;

		if (selected || changed)
		{
			ImGui::PushStyleColor(ImGuiCol_Border, selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
				: ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
		}

		if (ImGui::ColorButton("##swatch", color,
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, Ui::Scaled(22.0f, 22.0f)))
		{
			m_selected = i;

			if (!m_editEffect)
				StartFlash();
		}

		if (selected || changed)
		{
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
		}

		if (!m_editEffect && ImGui::IsItemHovered())
			ImGui::SetTooltip("entry %d%s", i, changed ? " - changed" : "");

		if (((n + 1) % 14) != 0 && n + 1 < count)
			ImGui::SameLine();

		ImGui::PopID();
	}
}

void PaletteEditorWindow::SyncEffectSwatches()
{
	for (int entry = 1; entry < PaletteFile::kColors; ++entry)
	{
		unsigned char observed[3] = {};

		const bool seen = EffectPaint::GetObserved(m_player, entry, observed);

		if (seen)
			memcpy(&m_original[entry * 4], observed, sizeof(observed));

		unsigned char edit[3] = {};

		if (EffectPaint::GetEdit(m_player, entry, edit))
			memcpy(&m_colors[entry * 4], edit, sizeof(edit));
		else if (seen)
			memcpy(&m_colors[entry * 4], observed, sizeof(observed));
	}
}

void PaletteEditorWindow::DrawEffectEntries()
{
	const int chara = PaletteManager::GetCharaNumber(m_player);

	SyncEffectSwatches();

	unsigned char entries[PaletteFile::kColors] = {};
	int entryCount = 0;

	for (int entry = 1; entry < PaletteFile::kColors; ++entry)
	{
		if (PartColourTable::IsPartEntry(chara, entry))
			entries[entryCount++] = static_cast<unsigned char>(entry);
	}

	if (entryCount == 0)
		return;

	ImGui::BeginChild("effectlist", Ui::Scaled(0.0f, 120.0f), ImGuiChildFlags_Borders);
	DrawSwatches(entries, entryCount);
	ImGui::EndChild();

	if (m_selected <= 0 || m_selected >= PaletteFile::kColors)
		return;

	float picked[3] = {
		m_colors[m_selected * 4 + 0] / 255.0f,
		m_colors[m_selected * 4 + 1] / 255.0f,
		m_colors[m_selected * 4 + 2] / 255.0f,
	};

	Ui::SetItemWidth(170.0f);
	if (ImGui::ColorPicker3("##effectcolor", picked,
		ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex |
		ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
	{
		m_colors[m_selected * 4 + 0] = static_cast<uint8_t>(picked[0] * 255.0f + 0.5f);
		m_colors[m_selected * 4 + 1] = static_cast<uint8_t>(picked[1] * 255.0f + 0.5f);
		m_colors[m_selected * 4 + 2] = static_cast<uint8_t>(picked[2] * 255.0f + 0.5f);

		EffectPaint::SetEntry(m_player, m_selected, &m_colors[m_selected * 4]);

		PaletteManager::NoteHandEdited(m_player);
	}
}

void PaletteEditorWindow::Push()
{
	EndFlash();

	int texture = 0;
	int slot = 0;

	if (!ActiveTarget(texture, slot))
		return;

	PaletteTexture::WriteRowKeepingAlpha(texture, static_cast<unsigned>(slot), m_colors);

	PaletteManager::NoteHandEdited(m_player);
}

void PaletteEditorWindow::LoadCreator()
{
	if (m_creatorLoaded)
		return;

	m_creatorLoaded = true;

	GetPrivateProfileStringA("Palette", "Creator", "", m_creator, sizeof(m_creator),
		Settings::GetIniPath().c_str());
}

void PaletteEditorWindow::CurrentColours(uint8_t* out) const
{
	const int texture = PaletteTexture::FindForPlayer(m_player);
	const int row = Row();

	if (texture >= 0 && row >= 0 &&
		PaletteTexture::ReadRowAsRgba(texture, static_cast<unsigned>(row), out))
	{
		return;
	}

	memcpy(out, m_colors, PaletteFile::kBytes);
}

void PaletteEditorWindow::Save()
{
	const int chara = PaletteManager::GetCharaNumber(m_player);
	if (chara < 0)
		return;

	Settings::SaveString("Palette", "Creator", m_creator);

	const char* const folder = PaletteManager::GetCharaName(chara);

	PaletteFile::Info info = {};
	strncpy_s(info.name, m_fileName, _TRUNCATE);
	strncpy_s(info.creator, m_creator, _TRUNCATE);
	strncpy_s(info.description, m_description, _TRUNCATE);

	const std::string path = GetModPalettePath(std::string(folder) + "\\" + m_fileName +
		PaletteFile::kExtension);

	uint8_t character[PaletteFile::kBytes] = {};
	uint8_t effect[PaletteFile::kBytes] = {};

	CurrentColours(character);

	EffectPaint::GetBlock(m_player, effect);
	const bool haveEffect = EffectPaint::GetEditedCount(m_player) > 0;

	if (PaletteFile::Save(path, character, info, haveEffect ? effect : nullptr))
	{
		sprintf_s(m_status, "saved to Palettes\\%s\\%s%s", folder, m_fileName,
			PaletteFile::kExtension);
		PaletteManager::Refresh();
	}
	else
	{
		sprintf_s(m_status, "could not write Palettes\\%s\\%s%s", folder, m_fileName,
			PaletteFile::kExtension);
	}

	LOG("palette editor: %s", m_status);
}

void PaletteEditorWindow::EndFlash()
{
	if (m_flashFrames <= 0)
	{
		m_flashTexture = 0;
		m_flashRow = -1;
		return;
	}

	m_flashFrames = 0;

	const int texture = m_flashTexture != 0 ? PaletteTexture::FindByPointer(m_flashTexture) : -1;

	if (texture >= 0 && m_flashRow >= 0)
		PaletteTexture::WriteRowKeepingAlpha(texture, static_cast<unsigned>(m_flashRow), m_colors);

	m_flashTexture = 0;
	m_flashRow = -1;
}

void PaletteEditorWindow::StartFlash()
{
	if (!m_flash)
	{
		EndFlash();
		return;
	}

	EndFlash();

	const int texture = PaletteTexture::FindForPlayer(m_player);
	if (texture < 0)
		return;

	m_flashTexture = PaletteTexture::GetSeen(texture);
	m_flashRow = Row();
	m_flashFrames = 90;
}

void PaletteEditorWindow::Flash()
{
	if (m_flashFrames <= 0)
		return;

	--m_flashFrames;

	uint8_t colors[PaletteFile::kBytes] = {};
	memcpy(colors, m_colors, sizeof(colors));

	if (m_flashFrames > 0)
	{
		for (int i = 0; i < PaletteFile::kColors; ++i)
		{
			const int grey = (m_colors[i * 4 + 0] * 30 + m_colors[i * 4 + 1] * 59 +
				m_colors[i * 4 + 2] * 11) / 100;

			colors[i * 4 + 0] = static_cast<uint8_t>(grey / 2);
			colors[i * 4 + 1] = static_cast<uint8_t>(grey / 2);
			colors[i * 4 + 2] = static_cast<uint8_t>(grey / 2);
		}

		const bool on = (m_flashFrames / 8) % 2 == 0;
		colors[m_selected * 4 + 0] = on ? 255 : 0;
		colors[m_selected * 4 + 1] = on ? 0 : 255;
		colors[m_selected * 4 + 2] = 255;
	}

	const int texture = m_flashTexture != 0 ? PaletteTexture::FindByPointer(m_flashTexture) : -1;

	if (texture >= 0 && m_flashRow >= 0)
		PaletteTexture::WriteRowKeepingAlpha(texture, static_cast<unsigned>(m_flashRow), colors);

	if (m_flashFrames == 0)
	{
		m_flashTexture = 0;
		m_flashRow = -1;
	}
}

void PaletteEditorWindow::Select(int player, bool effects)
{
	if (player == m_player && effects == m_editEffect)
		return;

	EndFlash();

	m_player = player;
	m_editEffect = effects;
	m_pulled = false;
	m_selected = 0;
	m_status[0] = '\0';
}

void PaletteEditorWindow::Draw()
{

	ForgetIfListChanged();
	LoadCreator();

	Flash();

	if (!GameState::AllowsPalettes())
	{
		ImGui::TextDisabled("in a match only");
		return;
	}

	if (!ImGui::BeginTabBar("sides"))
		return;

	for (int player = 0; player < 2; ++player)
	{
		const int chara = PaletteManager::GetCharaNumber(player);

		char label[64] = {};
		sprintf_s(label, "P%d  %s###side%d", player + 1, PaletteManager::GetCharaName(chara),
			player);

		if (!ImGui::BeginTabItem(label))
			continue;

		DrawSide(player);
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void PaletteEditorWindow::DrawSide(int player)
{
	if (PaletteTexture::FindForPlayer(player) < 0)
	{
		ImGui::TextDisabled("no palette texture for this side yet - the characters have to have drawn "
			"once");
		return;
	}

	if (!ImGui::BeginTabBar("views"))
		return;

	if (ImGui::BeginTabItem("Character colors"))
	{
		Select(player, false);
		DrawCharacterColors();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Effect colors"))
	{
		Select(player, true);
		DrawEffectColors();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void PaletteEditorWindow::DrawUndo(const char* label, const char* tip)
{
	if (ImGui::Button(label))
	{
		if (m_editEffect)
			EffectPaint::Clear(m_player);
		else
			PaletteManager::Restore(m_player, false);

		Pull();
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", tip);
}

void PaletteEditorWindow::DrawEffectColors()
{
	if (!m_pulled)
		Pull();

	if (!m_pulled)
		return;

	DrawUndo("Undo the effect colors",
		"Puts every effect color on this side back to the one its palette gives it.");

	DrawEffectEntries();
	DrawSaving();
}

void PaletteEditorWindow::DrawCharacterColors()
{
	if (!m_pulled)
		Pull();

	if (!m_pulled)
	{
		ImGui::TextDisabled("nothing to edit yet");
		return;
	}

	DrawUndo("Undo the character colors",
		"Puts this side's own colors back to the palette it is wearing. Effect colors are on "
		"their own tab and are left alone.");

	ImGui::Checkbox("Flash the entry on the character", &m_flash);

	const int chara = PaletteManager::GetCharaNumber(m_player);
	const int parts = GameTables::GetPartCount(chara);

	if (parts > 0)
	{
		ImGui::SameLine();
		ImGui::Checkbox("By part", &m_byPart);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Groups the entries the way the game's own color screen does - hair, "
				"skin, boots - out of its color-edit table rather than by guessing which entries "
				"look like padding.");
		}
	}

	if (parts == 0 || !m_byPart)
	{
		ImGui::SameLine();
		ImGui::Checkbox("Filter colors", &m_hideUnused);
	}

	ImGui::Separator();

	ImGui::BeginChild("swatches", Ui::Scaled(0.0f, 190.0f), ImGuiChildFlags_Borders);

	if (parts > 0 && m_byPart)
	{
		bool covered[PaletteFile::kColors] = {};

		for (int part = 0; part < parts; ++part)
		{
			const char* name = nullptr;
			const unsigned char* entries = nullptr;
			int count = 0;

			if (!GameTables::GetPart(chara, part, name, entries, count))
				continue;

			ImGui::PushID(part);

			if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen))
				DrawSwatches(entries, count);

			for (int i = 0; i < count; ++i)
				covered[entries[i]] = true;

			ImGui::PopID();
		}

		unsigned char rest[PaletteFile::kColors] = {};
		int restCount = 0;

		for (int i = 0; i < PaletteFile::kColors; ++i)
		{
			if (!covered[i] && IsUsed(chara, i))
				rest[restCount++] = static_cast<unsigned char>(i);
		}

		ImGui::PushID(parts);

		if (restCount > 0 && ImGui::CollapsingHeader("Everything else"))
			DrawSwatches(rest, restCount);

		ImGui::PopID();
	}
	else
	{
		bool offered[PaletteFile::kColors] = {};
		OfferedEntries::Fill(chara, offered, PaletteFile::kColors);

		unsigned char flat[PaletteFile::kColors] = {};
		int count = 0;

		for (int i = 0; i < PaletteFile::kColors; ++i)
		{
			if (!m_hideUnused || offered[i] || IsUsed(chara, i))
				flat[count++] = static_cast<unsigned char>(i);
		}

		if (count == 0)
			ImGui::TextDisabled("every entry looks like padding - untick Filter colors");
		else
			DrawSwatches(flat, count);
	}

	ImGui::EndChild();

	ImGui::Text("entry %d%s", m_selected,
		memcmp(m_colors + m_selected * 4, m_original + m_selected * 4, 3) != 0 ? "  (changed)" : "");

	float picked[3] = {
		m_colors[m_selected * 4 + 0] / 255.0f,
		m_colors[m_selected * 4 + 1] / 255.0f,
		m_colors[m_selected * 4 + 2] / 255.0f
	};

	Ui::SetItemWidth(170.0f);
	if (ImGui::ColorPicker3("##picker", picked, ImGuiColorEditFlags_NoSidePreview |
		ImGuiColorEditFlags_NoSmallPreview))
	{
		m_colors[m_selected * 4 + 0] = static_cast<uint8_t>(picked[0] * 255.0f + 0.5f);
		m_colors[m_selected * 4 + 1] = static_cast<uint8_t>(picked[1] * 255.0f + 0.5f);
		m_colors[m_selected * 4 + 2] = static_cast<uint8_t>(picked[2] * 255.0f + 0.5f);

		Push();
	}

	DrawSaving();
}

void PaletteEditorWindow::DrawSaving()
{
	ImGui::Separator();

	Ui::SetItemWidth(220.0f);
	ImGui::InputText("File name", m_fileName, sizeof(m_fileName));

	Ui::SetItemWidth(220.0f);
	ImGui::InputText("Creator", m_creator, sizeof(m_creator));

	Ui::SetItemWidth(220.0f);
	ImGui::InputText("Description", m_description, sizeof(m_description));

	if (ImGui::Button("Save"))
		Save();

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Writes a .pal into this character's folder with both tabs' colors as "
			"they are now - the character's own and any effect colors changed on this side. It is "
			"the game's own palette format, so it opens in Hantei-kun too.");
	}

	if (m_status[0] != '\0')
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", m_status);
	}
}
