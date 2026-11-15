#include "Palette/PaletteCatalog.h"

#include <Windows.h>

#include <cstring>

namespace {

std::string TitleOf(const char* file, const PaletteFile::Info& info)
{
	if (info.name[0] != '\0')
		return info.name;

	const std::string name = file;

	return name.substr(0, name.size() - strlen(PaletteFile::kExtension));
}

void ReadPalette(const std::string& folder, const char* file, PaletteCatalog::Character& out)
{
	PaletteCatalog::Palette palette;

	palette.file = file;
	palette.info = {};
	palette.colours.resize(PaletteFile::kBytes);

	if (!PaletteFile::Load(folder + "\\" + file, palette.colours.data(), palette.info))
		return;

	palette.title = TitleOf(file, palette.info);
	out.palettes.push_back(std::move(palette));
}

void ReadFolder(const PaletteCatalog::Folder& folder, PaletteCatalog::Character& out)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder.path + "\\*" + PaletteFile::kExtension).c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && !PaletteFile::IsCompanion(found.cFileName))
			ReadPalette(folder.path, found.cFileName, out);
	}
	while (FindNextFileA(search, &found));

	FindClose(search);
}

}

std::vector<PaletteCatalog::Character> PaletteCatalog::Read(const std::vector<Folder>& folders)
{
	std::vector<Character> characters;

	for (const Folder& folder : folders)
	{
		Character character = { folder.chara, folder.name, {} };

		ReadFolder(folder, character);

		if (!character.palettes.empty())
			characters.push_back(std::move(character));
	}

	return characters;
}
