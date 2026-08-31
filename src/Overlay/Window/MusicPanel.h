#pragma once

#include "Core/AsyncFileDialog.h"
#include "Game/BgmRules.h"
#include "Game/BgmThemes.h"
#include "Game/UserMusic.h"

#include <vector>

class MusicPanel
{
public:
	void Draw();

private:
	void DrawStatus();
	void DrawSoundpacks();
	bool DrawSoundpackRow(const BgmThemes::Theme& theme, bool isActive);
	int DrawSoundpackTable(int count, int active);
	void DrawSoundpackControls(int active);
	void DrawPackBuilder();
	void DrawPackDraft();
	void DrawPackDraftTable(int count);
	void DrawMyMusic();
	void RefreshMusicList();
	void DrawMyMusicRow(const UserMusic::Entry& entry);
	void DrawMyMusicTable();
	void DrawGetOst();
	void DrawTransfer();
	void DrawShuffle();
	void DrawBrowse();
	void DrawTrackCount();
	void SetUpTrackColumns(bool building);
	void DrawTrackRow(int id, const char* name, bool building, bool playing);
	void DrawTrackTable();
	void DrawRules();
	void DrawRuleTransfer();
	bool DrawRuleRow(int index, const BgmRules::Rule& stored);
	int DrawRuleTable(int count);
	void DrawRuleEditor();

	void DrawSourceFilter();
	bool PassesFilter(int id, const char* name) const;

	bool DrawTrackCombo(const char* label, int& bgm, bool sceneOnly = false);
	bool DrawTrackComboList(bool sceneOnly, int& bgm);
	bool DrawCharacterCombo(const char* label, int& chara);
	bool DrawSceneCombo(int& scene);

	BgmRules::Rule m_draft = { BgmRules::Kind_Matchup, 0, 1, 91, true, true };
	char m_search[64] = {};
	char m_pick[64] = {};
	AsyncFileDialog m_exportDialog;
	AsyncFileDialog m_importDialog;
	AsyncFileDialog m_ostDialog;
	AsyncFileDialog m_musicDialog;
	AsyncFileDialog m_rulesExportDialog;
	AsyncFileDialog m_rulesImportDialog;
	std::vector<UserMusic::Entry> m_music;
	int m_musicVersion = -1;
	char m_musicPack[32] = "My Music";
	char m_musicStatus[224] = {};
	char m_packStatus[224] = {};
	int m_source = 0;
};
