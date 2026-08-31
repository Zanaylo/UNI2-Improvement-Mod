#pragma once

#include "Overlay/Window/IWindow.h"
#include "Palette/PaletteFile.h"

#include <cstdint>

class PaletteEditorWindow : public IWindow
{
public:
	PaletteEditorWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags = 0);

protected:
	void BeforeDraw() override;
	void Draw() override;
	bool GrowsToFitContent() const override { return true; }

private:
	void Pull();
	void Push();
	void Save();
	void CurrentColours(uint8_t* out) const;
	void LoadCreator();
	void Flash();
	void StartFlash();
	void EndFlash();
	int Row() const;

	bool ActiveTarget(int& outTexture, int& outRow) const;
	bool IsUsed(int entry) const;
	void DrawSwatches(const unsigned char* entries, int count);

	void Select(int player, bool effects);
	void DrawSide(int player);
	void DrawCharacterColors();
	void DrawEffectColors();
	void DrawUndo(const char* label, const char* tip);
	void DrawSaving();

	void SyncEffectSwatches();
	void DrawEffectEntries();

	void ForgetIfListChanged();

	int m_player = 0;
	int m_selected = 0;
	bool m_pulled = false;
	bool m_editEffect = false;

	int m_generation = 0;
	int m_rowGeneration = 0;
	bool m_hideUnused = false;

	bool m_byPart = true;
	bool m_flash = true;
	int m_flashFrames = 0;

	int m_flashRow = -1;
	uintptr_t m_flashTexture = 0;

	uint8_t m_original[PaletteFile::kBytes] = {};

	uint8_t m_pristine[PaletteFile::kBytes] = {};

	uint8_t m_colors[PaletteFile::kBytes] = {};

	char m_fileName[64] = "my palette";
	char m_creator[PaletteFile::kCreatorLength] = {};
	bool m_creatorLoaded = false;
	char m_description[PaletteFile::kDescriptionLength] = {};
	char m_status[128] = {};

};
