#pragma once

#include <cstdint>

namespace EffectPaint
{
	constexpr int kPlayers = 2;
	constexpr int kColours = 256;

	bool Install();
	bool IsInstalled();

	bool GetObserved(int player, int entry, uint8_t* outRgb);
	int GetObservedCount(int player);

	void SetEntry(int player, int entry, const uint8_t* rgb);
	void ClearEntry(int player, int entry);
	void Clear(int player);

	constexpr int kBlockBytes = kColours * 4;

	void GetBlock(int player, uint8_t* block);
	void SetBlock(int player, const uint8_t* block);

	void SetRemote(int player, const uint8_t* block);
	void ClearRemote(int player);
	bool HasRemote(int player);
	bool GetRemoteEntry(int player, int entry, uint8_t* outRgb);

	void SetWear(int player, bool allowed);

	unsigned GetRevision(int player);

	bool IsEdited(int player, int entry);
	bool GetEdit(int player, int entry, uint8_t* outRgb);
	int GetEditedCount(int player);

	void PreviewObserved(int player, const uint8_t* rgb, int except, const uint8_t* exceptRgb);
	void EndPreview(int player);

	void Forget();

	bool Wants(int player, int entry);

	bool GetAnyObserved(int entry, uint8_t* outRgb);

	void SetForced(bool on, const uint8_t* rgb, int onlyEntry);
	bool IsForced();
	bool GetForced(uint8_t* outRgb, int& outEntry);
	int GetForcedCount();

	struct Call
	{
		int entry;
		uint8_t rgb[3];
		int calls;
		int substituted;
		int route;
		int answer;
	};

	int GetSeenCallCount();
	bool GetSeenCall(int index, Call& out);

	int GetSubstitutions(int player);

	int GetTintCalls();
	int GetBadIndex();
	int GetUnowned();
	int GetPassedThrough();
	int GetSuppressedByWear();

	void ResetCounts();
}
