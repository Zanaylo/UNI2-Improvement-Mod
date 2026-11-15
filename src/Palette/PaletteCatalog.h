#pragma once

#include "Palette/PaletteFile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PaletteCatalog
{
	struct Folder
	{
		int chara;
		std::string name;
		std::string path;
	};

	struct Palette
	{
		std::string file;
		std::string title;
		PaletteFile::Info info;
		std::vector<uint8_t> colours;
	};

	struct Character
	{
		int chara;
		std::string name;
		std::vector<Palette> palettes;
	};

	std::vector<Character> Read(const std::vector<Folder>& folders);
}
