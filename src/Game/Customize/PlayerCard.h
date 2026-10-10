#pragma once

#include "Game/Engine/GameOffsets.h"

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
	constexpr size_t kTitleMaxBytes = GameOffsets::kReplayTitleBytes - 1;

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
