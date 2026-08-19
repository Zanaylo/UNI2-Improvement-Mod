// The player card the game publishes to online opponents: one 0x4000 block of static memory that is
// copied verbatim both into SYS-DATA and into the opponent's player-slot record.
//
// Two rules cost something to learn and everything here depends on them. Nothing is written to disk
// unless the game's own save-needed byte is set, and the sanitiser that runs inside the save builder
// clears the title only when the preset title id is non-zero - so a free-text title survives exactly

#pragma once

#include <string>

namespace PlayerCard
{
	enum class PlateLayer
	{
		Frame,
		Panel,
		Chara,
		Base,
		Count
	};

	constexpr int kLayerCount = static_cast<int>(PlateLayer::Count);
	constexpr size_t kTitleMaxBytes = 0x3f;

	bool IsAvailable();

	const char* LayerName(PlateLayer layer);
	const char* LayerAssetPrefix(PlateLayer layer);

	bool GetPlate(PlateLayer layer, int& outId);
	bool SetPlate(PlateLayer layer, int id);
	bool EquipPlate(PlateLayer layer, int id);

	bool IsOwned(PlateLayer layer, int id);
	bool AddOwned(PlateLayer layer, int id);
	int GetOwnedCount(PlateLayer layer);

	bool GetTitle(std::string& outUtf8);
	bool SetTitle(const std::string& utf8, bool* outTruncated = nullptr, bool* outLossy = nullptr);

	bool GetIp(int& outIp);

	void MarkChanged();
}
