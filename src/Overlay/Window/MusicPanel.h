#pragma once

#include "Core/AsyncFileDialog.h"
#include "Game/BgmRules.h"

class MusicPanel
{
public:
	void Draw();

private:
	void DrawStatus();
	void DrawSoundpacks();
	void DrawGetOst();
	void DrawTransfer();
	void DrawShuffle();
	void DrawBrowse();
	void DrawRules();
	void DrawRuleEditor();

	void DrawSourceFilter();
	bool PassesFilter(int id, const char* name) const;

	bool DrawTrackCombo(const char* label, int& bgm);
	bool DrawCharacterCombo(const char* label, int& chara);

	BgmRules::Rule m_draft = { BgmRules::Kind_Matchup, 0, 1, 91, true, true };
	char m_search[64] = {};
	char m_pick[64] = {};
	AsyncFileDialog m_exportDialog;
	AsyncFileDialog m_importDialog;
	AsyncFileDialog m_ostDialog;
	AsyncFileDialog m_rulesExportDialog;
	AsyncFileDialog m_rulesImportDialog;
	int m_source = 0;
};
