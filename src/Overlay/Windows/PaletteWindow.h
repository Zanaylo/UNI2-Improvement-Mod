#pragma once

#include "Core/AsyncFileDialog.h"
#include "Game/Customize/LivePalette.h"
#include "Overlay/Framework/IWindow.h"

#include <string>

class PaletteWindow : public IWindow
{
public:
	PaletteWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;

private:
	static constexpr int kUndoDepth = 32;

	void DrawPlayer(int player);
	void DrawParts(int player);
	void DrawGrid(int player, const unsigned char* entries, int count);
	void DrawSwatches(int player, int chara);
	void DrawGroupedSwatches(int player, int chara, int parts);
	void DrawFlatSwatches(int player, int chara);
	void DrawPicker(int player);
	void DrawEffects(int player);
	void DrawFiles(int player);

	void PullBaseline(int player, bool force);
	void ComposeCompanion(int player);

	void DrawSummon(int player);
	void DrawSummonGrid(int player, const unsigned char* entries, int count);
	void DrawSummonPicker(int player);
	bool IsSummonJunk(int player, int entry) const;

	void Record(int player);
	void Undo(int player);

	void Apply(int player);
	void Refresh(int player);
	void FollowPartTints(int player);
	void ForgetPartTints(int player);

	void ApplyImportedColours(int player, const uint8_t* colours, const uint8_t* effects);
	void ApplyImportedSummon(int player, const uint8_t* colours);

	bool Save(int player);
	bool Load(int player, const char* name);
	void LoadSummon(int player, const char* name);
	bool AnySummonEdits(int player) const;
	void RefreshFiles(int player);

	void PollPngDialogs(int player);
	void CompleteImportPng(int player, const std::string& path);
	void CompleteExportPng(int player, const std::string& path);

	void Adopt(int player, int chara);

	void StartFlash(int player, bool summon);
	void RunFlash();

	LivePalette::Colours m_colours[2] = {};
	uint8_t m_composed[2][LivePalette::kBytes] = {};

	LivePalette::Colours m_history[2][kUndoDepth] = {};
	int m_historyCount[2] = {};

	int m_chara[2] = { -1, -1 };
	unsigned m_generation[2] = {};
	bool m_applied[2] = {};
	int m_selected[2] = { 1, 1 };
	int m_effectEntry[2] = { -1, -1 };
	bool m_pulled[2] = {};
	bool m_tinted[2][LivePalette::kColours] = {};

	uint8_t m_companion[2][LivePalette::kBytes] = {};
	uint8_t m_companionBase[2][LivePalette::kBytes] = {};
	bool m_hasCompanion[2] = {};
	bool m_summonTabOpen[2] = {};
	bool m_importIntoSummon[2] = {};

	struct SummonEdit
	{
		uint8_t entry[LivePalette::kColours][3];
		bool edited[LivePalette::kColours];
	};

	SummonEdit m_summon[2] = {};
	SummonEdit m_summonHistory[2][kUndoDepth] = {};
	int m_companionSelected[2] = { 1, 1 };
	bool m_flashSummon = false;

	char m_name[2][48] = {};
	char m_creator[2][32] = {};
	char m_description[2][64] = {};
	bool m_creatorLoaded = false;
	std::string m_files[2][64];
	int m_fileCount[2] = {};
	int m_chosen[2] = { -1, -1 };
	char m_status[2][96] = {};

	AsyncFileDialog m_pngImportDialog[2];
	AsyncFileDialog m_pngExportDialog[2];

	void LoadCreator();
	void Bare(int player);
	void SelectFile(int player, const char* file);
	bool IsJunk(int player, int chara, int entry) const;

	int m_flashFrames = 0;
	int m_flashPlayer = -1;
};
