#pragma once

#include "Core/BackgroundJob.h"
#include "Overlay/Framework/IWindow.h"
#include "Overlay/Widgets/PaletteThumbnails.h"
#include "Palette/PaletteCatalog.h"

#include <atomic>
#include <string>
#include <vector>

class PaletteLibraryWindow : public IWindow
{
public:
	PaletteLibraryWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;
	void OnClosed() override;

private:
	static constexpr int kPlayers = 2;

	void StartScan();
	void TakeScan();
	void FindSelected();
	void ReadWorn();

	void DrawToolbar();
	void DrawCharacters();
	void DrawCharacter(const PaletteCatalog::Character& character);
	void DrawCards(const PaletteCatalog::Character& character);
	void DrawRow(const PaletteCatalog::Character& character, int first, int end, const ImVec2& cell);
	void DrawCard(const PaletteCatalog::Character& character, const PaletteCatalog::Palette& palette, const ImVec2& cell);
	void DrawBadge(ImDrawList* draw, int chara, const char* file, const ImVec2& corner, float width) const;

	void DrawDetails();
	void DrawWearButtons(const PaletteCatalog::Character& character, const PaletteCatalog::Palette& palette);

	void CollectMatches(const PaletteCatalog::Character& character);
	bool IsWorn(int chara, const char* file) const;

	std::vector<PaletteCatalog::Character> m_characters;
	std::vector<PaletteCatalog::Character> m_scanned;
	std::vector<const PaletteCatalog::Palette*> m_matches;

	BackgroundJob m_scanJob;
	std::atomic<bool> m_wantsScan = false;
	bool m_scanStarted = false;

	PaletteThumbnails m_thumbnails;

	const PaletteCatalog::Character* m_selectedCharacter = nullptr;
	const PaletteCatalog::Palette* m_selected = nullptr;
	int m_selectedChara = -1;
	std::string m_selectedFile;

	int m_wornChara[kPlayers] = { -1, -1 };
	const char* m_wornFile[kPlayers] = {};

	char m_filter[64] = {};
};
