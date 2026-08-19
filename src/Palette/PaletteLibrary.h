#pragma once

#include <string>

namespace PaletteLibrary
{
	constexpr int kMaxFiles = 64;

	std::string FolderFor(int chara);

	void Rescan(int chara);

	int GetCount(int chara);
	const char* GetName(int chara, int index);
}
