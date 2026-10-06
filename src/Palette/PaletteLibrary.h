#pragma once

#include <cstdint>
#include <string>

namespace PaletteLibrary
{
	constexpr int kMaxFiles = 64;

	std::string FolderFor(int chara);
	std::string PathOf(int chara, const char* file);

	bool LoadWithEffects(int chara, const char* file, uint8_t* rgba, uint8_t* effects, bool* outHasEffects = nullptr);

	void Rescan(int chara);

	int GetCount(int chara);
	const char* GetName(int chara, int index);
	uint64_t GetCreated(int chara, int index);
}
