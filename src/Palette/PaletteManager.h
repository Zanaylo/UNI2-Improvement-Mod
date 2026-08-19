#pragma once

#include <cstdint>

namespace PaletteManager
{
	constexpr int kMaxPalettes = 64;

	void Refresh();

	int GetCharaNumber(int player);
	const char* GetCharaName(int chara);

	int GetCount(int player);
	const char* GetName(int player, int index);
	const char* GetCreator(int player, int index);

	int GetApplied(int player);
	bool Apply(int player, int index);

	void NoteHandEdited(int player);
	bool IsHandEdited(int player);

	bool Restore(int player, bool alsoEffects = true);

	const uint8_t* GetAppliedColors(int player);
	const uint8_t* GetAppliedEffectColors(int player);
	const char* GetAppliedName(int player);

	bool ApplyForeign(int player, const uint8_t* colors, const char* name,
		const uint8_t* effectColors);
	bool HasForeign(int player);
	const uint8_t* GetForeignColors(int player);
	const char* GetForeignName(int player);
	void ClearForeign();

	bool FindEffectRow(int player, int& outTexture, int& outRow);

	void LogTextureReport(int player);

	void OnFrame();

	void Reapply();

	void RepaintSides();

	void DetectSwapSides();
}
