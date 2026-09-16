#pragma once

#include <cstdint>

namespace PalettePaint
{
	constexpr int kPlayers = 2;
	constexpr int kColours = 256;
	constexpr int kBytes = kColours * 4;

	constexpr unsigned kSets = 2;
	constexpr unsigned kRows = kSets * 2;

	void Stage(int player, const uint8_t* colours);
	void Clear(int player);

	void StageCompanion(int player, const uint8_t* colours);
	void ClearCompanion(int player);
	void PreviewCompanion(int player, const uint8_t* colours);
	bool HasCompanion(int player);

	bool ReadCompanionColours(int player, uint8_t* rgba);

	bool IsStaged(int player);

	void Preview(int player, const uint8_t* colours);
	void EndPreview(int player);

	void StageRemote(int player, const uint8_t* colours);
	void ClearRemote(int player);
	bool HasRemote(int player);

	const uint8_t* GetRemote(int player);

	void OnFrame();

	void OnDraw();

	bool IsPainting(int player);

	bool ReadGameColours(int player, uint8_t* rgba);

	const uint8_t* GetStaged(int player);

	unsigned GetRevision(int player);

	int GetWrites(int player);
	int GetIndex(int player);

	uintptr_t GetOwner(int player);

	int GetInnerOffset();
}
