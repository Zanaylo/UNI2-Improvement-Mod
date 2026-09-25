#include "Overlay/Panels/SubtitlesPanel.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Game/Tables/CharaTables.h"
#include "Game/Subtitles/SubtitleText.h"
#include "Game/Subtitles/SubtitleWatch.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr float kTableHeight = 320.0f;
constexpr float kNoteWidth = 240.0f;
constexpr const char* kUnwritable = "that file could not be written";

bool Matches(const SubtitleTable::Line& line, const char* filter)
{
	if (filter[0] == 0)
		return true;

	return strstr(line.stem, filter) != nullptr || strstr(line.note, filter) != nullptr ||
		strstr(line.text, filter) != nullptr;
}

}

void SubtitlesPanel::Draw()
{
	DrawSwitch();

	if (!SubtitleWatch::IsAvailable())
	{
		UiText::Warn("%s", SubtitleWatch::StatusText());
		return;
	}

	ImGui::Separator();

	DrawFile();

	ImGui::Separator();

	DrawLook();

	ImGui::Separator();

	if (SubtitleTable::Chosen()[0] == 0)
	{
		CancelEdit();
		UiText::Muted("Choose or make a subtitle file above to start writing lines.");
		return;
	}

	DrawPicker();
	DrawRows();
}

void SubtitlesPanel::DrawSwitch()
{
	ImGui::TextUnformatted("Subtitles");
	UiText::Help("Shows what characters say as text on screen. If the game already has a subtitle "
		"for a voice line, the game draws your text with its own font, position and timing. Every "
		"other line is drawn by the overlay as soon as it plays.");

	bool on = SubtitleWatch::IsEnabled();

	if (ImGui::Checkbox("Show subtitles", &on))
	{
		SubtitleWatch::SetEnabled(on);
		g_modVals.subtitles = on;
		Settings::SaveInt("Subtitles", "Subtitles", on ? 1 : 0);
	}

	ImGui::SameLine();
	UiText::Muted("%d line(s) shown this session", SubtitleWatch::Shows());

	DrawGameDrawn();
}

void SubtitlesPanel::DrawGameDrawn()
{
	if (!SubtitleText::IsAvailable())
	{
		UiText::Warn("%s", SubtitleText::StatusText());
		return;
	}

	UiText::Muted("Also turn on the game's own Subtitles option (Options, then Game settings). The "
		"game checks it before drawing a line. This session it asked for %d line(s) and drew %d "
		"of yours.", SubtitleText::Asked(), SubtitleText::Answers());

	UiText::Muted("The game's font only has Western characters, so accents work but kanji don't.");
}

void SubtitlesPanel::DrawFile()
{
	const char* const chosen = SubtitleTable::Chosen();
	const char* const label = chosen[0] != 0 ? chosen : "None";

	ImGui::SetNextItemWidth(Ui::Scaled(240.0f));

	if (ImGui::BeginCombo("Subtitle file", label))
	{
		if (ImGui::Selectable("None", chosen[0] == 0))
		{
			CommitEdit();
			SubtitleTable::Choose("");
			Settings::SaveString("Subtitles", "File", "");
			m_visibleChara = -1;
		}

		for (int i = 0; i < SubtitleTable::PackCount(); ++i)
		{
			const char* const name = SubtitleTable::PackAt(i);

			if (ImGui::Selectable(name, strcmp(name, chosen) == 0))
			{
				CommitEdit();
				SubtitleTable::Choose(name);
				Settings::SaveString("Subtitles", "File", name);
				m_visibleChara = -1;
			}
		}

		ImGui::EndCombo();
	}

	UiText::Muted("%s", SubtitleTable::StatusText());
	UiText::Muted("%s", SubtitleTable::FolderPath());

	DrawNewFile();
	DrawTransfer();
}

void SubtitlesPanel::DrawNewFile()
{
	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));
	ImGui::InputTextWithHint("##newsubtitles", "New file name", m_newName, sizeof(m_newName));

	ImGui::SameLine();

	ImGui::BeginDisabled(m_newName[0] == 0);

	if (ImGui::Button("Create"))
	{
		CommitEdit();

		if (SubtitleTable::Create(m_newName))
		{
			Settings::SaveString("Subtitles", "File", m_newName);
			sprintf_s(m_status, "'%s' created", m_newName);
			m_newName[0] = 0;
			m_visibleChara = -1;
		}
		else
		{
			strncpy_s(m_status, kUnwritable, _TRUNCATE);
		}
	}

	ImGui::EndDisabled();
}

void SubtitlesPanel::DrawTransfer()
{
	const bool chosen = SubtitleTable::Chosen()[0] != 0;

	ImGui::BeginDisabled(!chosen);

	if (ImGui::SmallButton("Export"))
	{
		CommitEdit();
		m_export.BeginSave("Export subtitles", "Subtitle file\0*.txt\0\0", "subtitles.txt");
	}

	ImGui::SameLine();

	if (ImGui::SmallButton("Import"))
		m_import.BeginOpen("Import subtitles", "Subtitle file\0*.txt\0\0");

	ImGui::EndDisabled();

	ImGui::SameLine();
	UiText::Help("Export saves the chosen file so you can share it. Import adds the lines from a "
		"file someone sent you to the chosen file. Lines for the same voice are replaced and the "
		"rest are kept.");

	std::string path;

	if (m_export.TakeResult(path))
	{
		strncpy_s(m_status, SubtitleTable::ExportTo(path.c_str()) ? "exported"
			: kUnwritable, _TRUNCATE);
	}

	if (m_import.TakeResult(path))
	{
		CommitEdit();
		strncpy_s(m_status, SubtitleTable::ImportFrom(path.c_str()) ? "imported"
			: "nothing could be read from that file", _TRUNCATE);
		m_visibleChara = -1;
	}

	if (m_status[0] != 0)
		UiText::Muted("%s", m_status);
}

void SubtitlesPanel::DrawLook()
{
	UiText::Muted("These settings change how the overlay draws subtitles for voices the game has "
		"no subtitle for.");

	int hold = SubtitleWatch::HoldMs();
	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));

	if (ImGui::SliderInt("Time on screen", &hold, 500, 8000, "%d ms"))
	{
		SubtitleWatch::SetHoldMs(hold);
		g_modVals.subtitleHoldMs = SubtitleWatch::HoldMs();
		Settings::SaveInt("Subtitles", "HoldMs", g_modVals.subtitleHoldMs);
	}

	int scale = g_modVals.subtitleScale;
	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));

	if (ImGui::SliderInt("Text size", &scale, 50, 400, "%d%%"))
	{
		g_modVals.subtitleScale = scale;
		Settings::SaveInt("Subtitles", "Scale", scale);
	}

	int y = g_modVals.subtitleY;
	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));

	if (ImGui::SliderInt("Height on screen", &y, 5, 98, "%d%%"))
	{
		g_modVals.subtitleY = y;
		Settings::SaveInt("Subtitles", "PositionY", y);
	}

	DrawSpeaker();
}

void SubtitlesPanel::DrawSpeaker()
{
	bool names = g_modVals.subtitleNames;

	if (!ImGui::Checkbox("Show who is speaking", &names))
		return;

	g_modVals.subtitleNames = names;
	Settings::SaveInt("Subtitles", "ShowSpeaker", names ? 1 : 0);
}

void SubtitlesPanel::DrawPicker()
{
	const char* const name = CharaTables::Name(m_chara);

	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));

	if (ImGui::BeginCombo("Character", name != nullptr ? name : "?"))
	{
		for (int i = 0; i < CharaTables::GetCharaCount(); ++i)
		{
			const char* const row = CharaTables::Name(i);

			if (row == nullptr)
				continue;

			if (ImGui::Selectable(row, i == m_chara))
			{
				CommitEdit();
				m_chara = i;
			}
		}

		ImGui::EndCombo();
	}

	if (!SubtitleTable::IsAttached(m_chara))
		SubtitleTable::Attach(m_chara);

	ImGui::SameLine();
	UiText::Muted("%d line(s), %d written", SubtitleTable::Rows(m_chara),
		SubtitleTable::Written(m_chara));

	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));
	ImGui::InputTextWithHint("##subtitlefilter", "Search", m_filter, sizeof(m_filter));

	ImGui::SameLine();
	ImGui::Checkbox("Only lines with a spoken note", &m_onlySpoken);

	ImGui::SameLine();
	ImGui::Checkbox("Only what I wrote", &m_onlyWritten);

	UiText::Muted("Click a subtitle to edit it. Click anywhere else to keep your text, or press "
		"Esc to undo.");
}

void SubtitlesPanel::UpdateVisible()
{
	if (m_visibleChara == m_chara && strcmp(m_visibleFilter, m_filter) == 0)
		return;

	m_visibleChara = m_chara;
	strncpy_s(m_visibleFilter, m_filter, _TRUNCATE);

	m_visible.clear();

	for (int i = 0; i < SubtitleTable::Rows(m_chara); ++i)
	{
		const SubtitleTable::Line* const line = SubtitleTable::Row(m_chara, i);

		if (line == nullptr || !Matches(*line, m_filter))
			continue;

		m_visible.push_back(i);
	}
}

void SubtitlesPanel::DrawRows()
{
	UpdateVisible();

	if (SubtitleTable::Rows(m_chara) == 0)
	{
		UiText::Warn("Couldn't read the game's chrNNN_se_list.txt for this character, so there are "
			"no lines to write.");
		return;
	}

	if (!ImGui::BeginTable("##subtitlerows", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
		ImVec2(0.0f, Ui::Scaled(kTableHeight))))
	{
		return;
	}

	ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(44.0f));
	ImGui::TableSetupColumn("Voice", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(150.0f));
	ImGui::TableSetupColumn("Spoken", ImGuiTableColumnFlags_WidthFixed, Ui::Scaled(kNoteWidth));
	ImGui::TableSetupColumn("Subtitle", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (int row : m_visible)
		DrawRow(row);

	ImGui::EndTable();
}

void SubtitlesPanel::DrawRow(int row)
{
	const SubtitleTable::Line* const line = SubtitleTable::Row(m_chara, row);

	if (line == nullptr)
		return;

	const bool editing = m_editing == row;

	if (!editing && m_onlySpoken && line->note[0] == 0)
		return;

	if (!editing && m_onlyWritten && line->text[0] == 0)
		return;

	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text("%d", line->index);

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(line->stem);

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(line->note[0] != 0 ? line->note : "-");

	ImGui::TableNextColumn();
	ImGui::PushID(row);

	if (!editing)
	{
		if (ImGui::Selectable(line->text[0] != 0 ? line->text : "write one"))
			BeginEdit(row, line->text);

		ImGui::PopID();
		return;
	}

	if (m_focusEdit)
	{
		ImGui::SetKeyboardFocusHere();
		m_focusEdit = false;
	}

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##subtitle", m_edit, sizeof(m_edit));

	const bool left = ImGui::IsItemDeactivated();

	ImGui::PopID();

	if (!left)
		return;

	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
	{
		CancelEdit();
		return;
	}

	CommitEdit();
}

void SubtitlesPanel::BeginEdit(int row, const char* text)
{
	CommitEdit();

	m_editing = row;
	m_focusEdit = true;
	strncpy_s(m_edit, text != nullptr ? text : "", _TRUNCATE);
}

void SubtitlesPanel::CommitEdit()
{
	if (m_editing < 0)
		return;

	const int row = m_editing;
	m_editing = -1;
	m_focusEdit = false;

	const SubtitleTable::Line* const line = SubtitleTable::Row(m_chara, row);

	if (line == nullptr || strcmp(line->text, m_edit) == 0)
		return;

	SubtitleTable::Write(m_chara, row, m_edit);
	SubtitleTable::Save();
}

void SubtitlesPanel::CancelEdit()
{
	m_editing = -1;
	m_focusEdit = false;
}
