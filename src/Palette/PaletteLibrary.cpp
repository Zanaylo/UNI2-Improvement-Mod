#include "Palette/PaletteLibrary.h"

#include "Core/utils.h"
#include "Palette/PaletteFile.h"
#include "Palette/PaletteManager.h"

#include <Windows.h>

namespace {

constexpr int kSlots = 2;

struct Shelf
{
	int chara;
	int count;
	std::string names[PaletteLibrary::kMaxFiles];
};

Shelf g_shelves[kSlots] = { { -1, 0, {} }, { -1, 0, {} } };
int g_next = 0;

void Fill(Shelf& shelf, int chara)
{
	shelf.chara = chara;
	shelf.count = 0;

	if (chara < 0)
		return;

	const std::string folder = PaletteLibrary::FolderFor(chara);
	CreateDirectoryA(folder.c_str(), nullptr);

	WIN32_FIND_DATAA found = {};
	const HANDLE search =
		FindFirstFileA((folder + "\\*" + PaletteFile::kExtension).c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (shelf.count >= PaletteLibrary::kMaxFiles)
			break;

		shelf.names[shelf.count++] = found.cFileName;
	}
	while (FindNextFileA(search, &found));

	FindClose(search);
}

Shelf& ShelfFor(int chara)
{
	for (Shelf& shelf : g_shelves)
	{
		if (shelf.chara == chara)
			return shelf;
	}

	Shelf& fresh = g_shelves[g_next];
	g_next = (g_next + 1) % kSlots;

	Fill(fresh, chara);
	return fresh;
}

}

std::string PaletteLibrary::FolderFor(int chara)
{
	return GetModPalettePath(PaletteManager::GetCharaName(chara));
}

void PaletteLibrary::Rescan(int chara)
{
	Fill(ShelfFor(chara), chara);
}

int PaletteLibrary::GetCount(int chara)
{
	return chara < 0 ? 0 : ShelfFor(chara).count;
}

const char* PaletteLibrary::GetName(int chara, int index)
{
	if (chara < 0 || index < 0)
		return "";

	const Shelf& shelf = ShelfFor(chara);

	return index < shelf.count ? shelf.names[index].c_str() : "";
}
