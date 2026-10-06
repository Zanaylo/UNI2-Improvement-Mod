#pragma once

#include <cstdint>
#include <string>

namespace PaletteLibrary
{
	constexpr int kMaxFiles = 64;

	std::string FolderFor(int chara);
	std::string PathOf(int chara, const char* file);

	bool LoadColours(int chara, const char* file, uint8_t* rgba);

	void Rescan(int chara);

	int GetCount(int chara);
	const char* GetName(int chara, int index);
	uint64_t GetCreated(int chara, int index);
}
