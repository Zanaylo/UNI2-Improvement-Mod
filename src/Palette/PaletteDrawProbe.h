#pragma once

#include <cstdint>

namespace PaletteDrawProbe
{
	bool Install();
	bool IsInstalled();

	void SetCapturing(bool on);
	bool IsCapturing();

	void OnSetTexture(unsigned stage, void* texture, bool paletteShaped);
	void OnDraw();
	void OnFrame();

	struct Tuple
	{
		uintptr_t texture;
		int stage;
		int row;
		bool mainPath;
		int count;
		int frames;
		int lastAgeFrames;

		uintptr_t chara;
	};

	int GetTupleCount();
	bool GetTuple(int index, Tuple& out);

	int GetCallsLastFrame();
	int GetDrawsLastFrame();
	int GetTuplesLastFrame();

	int GetCharaHits();
	int GetRowSets();

	void ResetTuples();

	bool GetSeatVotes(uintptr_t texture, unsigned& outFirst, unsigned& outSecond);

	void Dump();

	void SetNameCapture(bool enabled);
	bool IsNameCapturing();
	void DumpNames();

	struct Tint
	{
		uintptr_t texture;
		float rgb[3];
		int draws;

		uintptr_t valuesPointer;
		uintptr_t callerRva;

		int paletteIndex;

		uintptr_t stack[8];
		int stackCount;
	};

	void SetTintCapture(bool enabled);
	bool IsTintCapturing();
	int GetTintCount();
	bool GetTint(int index, Tint& out);
	void DumpTints();

	void SetForcedColor(bool enabled, const char* name, const float* rgb, uintptr_t onlyTexture = 0,
		int onlyIndex = -1);
	bool IsForcingColor();
	const char* GetForcedName();

	bool IsEffectEntryEdited(int player, int entry);

	bool GetEffectEntryEdit(int player, int entry, unsigned char* outRgb);

	bool GetEffectEntryColour(int player, int entry, unsigned char* outRgb,
		unsigned char* outFirst = nullptr);

	void SetEffectEntryColour(int player, int entry, const unsigned char* rgb);
	void ClearEffectEntryColours(int player);
	int GetEffectEntryEditedCount(int player);

	void ResetMatchState();

	void SyncEffectSides(int chara0, int palette0, int chara1, int palette1);

	void ResetEffectEntries();

	void GetObjectPoolCounts(int& outEffect, int& outChara, int& outElsewhere);
}
