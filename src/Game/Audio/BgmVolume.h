#pragma once

namespace BgmVolume
{
	constexpr int kFullPercent = 100;

	bool Install();

	void Load();
	void Save();

	int Get(int id);
	void Set(int id, int percent);

	bool IsCustom(int id);
	int CustomCount();
	void ResetAll();

	int EngineValue(int percent);

	void ApplyToSlot(int slot, int id);
	void SetCurrent(int slot, int id);
	void ApplyNow();
}
