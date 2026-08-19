#pragma once

#include "Game/ColorCustomize.h"
#include "Game/StockPalettes.h"
#include "Overlay/CharaPreview.h"

#include <cstdint>
#include <vector>

class ColorCustomizePanel
{
public:
	void Draw();

private:
	struct Edit
	{
		int values[ColorCustomize::kPartCount];
		int equipped;
	};

	void DrawCharacter();
	void DrawPose();
	void DrawPreview();
	void DrawSlot();
	void DrawPart(int part);
	void DrawActions();

	void Compose(uint8_t* out) const;
	const uint8_t* ReferenceShade(const uint8_t* palette, int part) const;
	void DrawPick(int part);
	void DrawExport();

	void Pull();
	void Save();
	void Undo();
	void Record();

	bool IsDirty() const;

	void StepColour(int part, int steps);
	void StepSlot(int steps);

	void SetStatus(const char* text);

	int PaletteCount() const;

	int m_chara = 0;
	int m_slot = 0;
	int m_frame = 0;
	int m_pulledChara = -1;
	int m_pulledSlot = -1;
	int m_hovered = -1;

	Edit m_edit = {};
	Edit m_saved = {};
	std::vector<Edit> m_history;

	char m_status[192] = {};

	CharaPreview m_preview;
};
