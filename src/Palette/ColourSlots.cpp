#include "Palette/ColourSlots.h"

#include "Core/Config/Settings.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PaletteManager.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* kSection = "PaletteSlots";
constexpr int kFileLength = 128;

struct Shelf
{
	bool loaded;
	std::string files[ColourSlots::kGameColours];
};

Shelf g_shelves[ColourSlots::kCharacters] = {};

bool IsCharacter(int chara)
{
	return chara >= 0 && chara < ColourSlots::kCharacters;
}

bool IsColour(int colour)
{
	return colour >= 0 && colour < ColourSlots::kGameColours;
}

bool IsFile(const char* file)
{
	return file != nullptr && file[0] != '\0';
}

void KeyFor(int chara, int colour, char* key, size_t size)
{
	sprintf_s(key, size, "%s.%d", PaletteManager::GetCharaName(chara), colour);
}

Shelf& ShelfFor(int chara)
{
	Shelf& shelf = g_shelves[chara];

	if (shelf.loaded)
		return shelf;

	const std::string path = Settings::GetIniPath();

	for (int colour = 0; colour < ColourSlots::kGameColours; ++colour)
	{
		char key[64] = {};
		char file[kFileLength] = {};

		KeyFor(chara, colour, key, sizeof(key));
		GetPrivateProfileStringA(kSection, key, "", file, sizeof(file), path.c_str());

		shelf.files[colour] = file;
	}

	shelf.loaded = true;
	return shelf;
}

void Store(int chara, int colour, const char* file)
{
	ShelfFor(chara).files[colour] = file;

	char key[64] = {};
	KeyFor(chara, colour, key, sizeof(key));

	Settings::SaveString(kSection, key, IsFile(file) ? file : nullptr);
}

bool IsBound(int chara, const char* file)
{
	return ColourSlots::ColourOf(chara, file) != ColourSlots::kNoColour;
}

int CreationOrder(int chara, int* order)
{
	const int count = (std::min)(PaletteLibrary::GetCount(chara), static_cast<int>(PaletteLibrary::kMaxFiles));

	for (int i = 0; i < count; ++i)
		order[i] = i;

	std::stable_sort(order, order + count, [chara](int a, int b)
	{
		return PaletteLibrary::GetCreated(chara, a) < PaletteLibrary::GetCreated(chara, b);
	});

	return count;
}

template <typename Visit>
int WalkExtended(int chara, Visit visit)
{
	if (!IsCharacter(chara))
		return ColourSlots::kNoExtended;

	int order[PaletteLibrary::kMaxFiles] = {};
	const int count = CreationOrder(chara, order);
	int slot = ColourSlots::kNoExtended;

	for (int i = 0; i < count; ++i)
	{
		const char* const name = PaletteLibrary::GetName(chara, order[i]);

		if (IsBound(chara, name))
			continue;

		if (visit(++slot, name))
			return slot;
	}

	return slot;
}

}

const char* ColourSlots::BoundFile(int chara, int colour)
{
	if (!IsCharacter(chara) || !IsColour(colour))
		return "";

	return ShelfFor(chara).files[colour].c_str();
}

int ColourSlots::ColourOf(int chara, const char* file)
{
	if (!IsCharacter(chara) || !IsFile(file))
		return kNoColour;

	const Shelf& shelf = ShelfFor(chara);

	for (int colour = 0; colour < kGameColours; ++colour)
	{
		if (_stricmp(shelf.files[colour].c_str(), file) == 0)
			return colour;
	}

	return kNoColour;
}

void ColourSlots::Bind(int chara, int colour, const char* file)
{
	if (!IsCharacter(chara) || !IsColour(colour) || !IsFile(file))
		return;

	Release(chara, file);
	Store(chara, colour, file);
}

void ColourSlots::Release(int chara, const char* file)
{
	const int colour = ColourOf(chara, file);

	if (colour == kNoColour)
		return;

	Store(chara, colour, "");
}

int ColourSlots::ExtendedCount(int chara)
{
	return WalkExtended(chara, [](int, const char*) { return false; });
}

const char* ColourSlots::ExtendedFile(int chara, int extended)
{
	const char* found = "";

	WalkExtended(chara, [&](int slot, const char* name)
	{
		if (slot != extended)
			return false;

		found = name;
		return true;
	});

	return found;
}

int ColourSlots::ExtendedOf(int chara, const char* file)
{
	if (!IsFile(file))
		return kNoExtended;

	int found = kNoExtended;

	WalkExtended(chara, [&](int slot, const char* name)
	{
		if (_stricmp(name, file) != 0)
			return false;

		found = slot;
		return true;
	});

	return found;
}

void ColourSlots::Reload()
{
	for (Shelf& shelf : g_shelves)
		shelf.loaded = false;
}
