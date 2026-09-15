#pragma once

#include "Core/AsyncFileDialog.h"
#include "Game/SubtitleTable.h"

#include <vector>

class SubtitlesPanel
{
public:
	void Draw();

private:
	void DrawSwitch();
	void DrawGameDrawn();
	void DrawFile();
	void DrawNewFile();
	void DrawTransfer();
	void DrawLook();
	void DrawSpeaker();
	void DrawPicker();
	void DrawRows();
	void DrawRow(int row);
	void UpdateVisible();

	void BeginEdit(int row, const char* text);
	void CommitEdit();
	void CancelEdit();

	AsyncFileDialog m_import;
	AsyncFileDialog m_export;

	std::vector<int> m_visible;

	int m_chara = 0;
	int m_visibleChara = -1;
	int m_editing = -1;
	bool m_focusEdit = false;
	bool m_onlyWritten = false;
	bool m_onlySpoken = true;
	char m_filter[64] = {};
	char m_visibleFilter[64] = {};
	char m_newName[SubtitleTable::kNameMax] = {};
	char m_edit[SubtitleTable::kTextMax] = {};
	char m_status[192] = {};
};
