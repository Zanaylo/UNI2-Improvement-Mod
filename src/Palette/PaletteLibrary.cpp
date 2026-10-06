#include "Palette/PaletteLibrary.h"

#include "Core/utils.h"
#include "Palette/EffectBlock.h"
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
	uint64_t created[PaletteLibrary::kMaxFiles];
};

Shelf g_shelves[kSlots] = { { -1, 0, {}, {} }, { -1, 0, {}, {} } };
int g_next = 0;

uint64_t TimeOf(const FILETIME& time)
{
	return static_cast<uint64_t>(time.dwHighDateTime) << 32 | time.dwLowDateTime;
}

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

		if (PaletteFile::IsCompanion(found.cFileName))
			continue;

		if (shelf.count >= PaletteLibrary::kMaxFiles)
			break;

		shelf.names[shelf.count] = found.cFileName;
		shelf.created[shelf.count] = TimeOf(found.ftCreationTime);
		++shelf.count;
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

std::string PaletteLibrary::PathOf(int chara, const char* file)
{
	return FolderFor(chara) + "\\" + file;
}

bool PaletteLibrary::LoadWithEffects(int chara, const char* file, uint8_t* rgba, uint8_t* effects, bool* outHasEffects)
{
	if (file == nullptr || file[0] == '\0' || rgba == nullptr || effects == nullptr)
		return false;

	PaletteFile::Info info = {};
	uint8_t page[PaletteFile::kBytes] = {};
	bool hasPage = false;

	if (!PaletteFile::Load(PathOf(chara, file), rgba, info, page, &hasPage))
		return false;

	const bool hasEffects = EffectBlock::Compose(chara, rgba, hasPage ? page : nullptr, effects);

	if (outHasEffects != nullptr)
		*outHasEffects = hasEffects;

	return true;
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

uint64_t PaletteLibrary::GetCreated(int chara, int index)
{
	if (chara < 0)
		return 0;

	const Shelf& shelf = ShelfFor(chara);

	return index >= 0 && index < shelf.count ? shelf.created[index] : 0;
}
