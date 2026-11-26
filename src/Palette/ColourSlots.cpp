#include "Palette/ColourSlots.h"

#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Game/Customize/ColorCustomize.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PaletteManager.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr const char* kSection = "PaletteSlots";
constexpr const char* kOrderKey = "ExtendedOrder";
constexpr char kOrderSeparator = '|';
constexpr size_t kSectionBytes = 32 * 1024;
constexpr DWORD kSectionTruncatedMargin = 2;

struct Shelf
{
	bool loaded;
	std::string files[ColourSlots::kGameColours];
	std::vector<std::string> customOrder;
};

Shelf g_shelves[ColourSlots::kCharacters] = {};
bool g_orderLoaded = false;
ColourSlots::ExtendedOrder g_extendedOrder = ColourSlots::ExtendedOrder::OldestFirst;

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

void OrderKeyFor(int chara, char* key, size_t size)
{
	sprintf_s(key, size, "%s.Order", PaletteManager::GetCharaName(chara));
}

const char* OrderToString(ColourSlots::ExtendedOrder order)
{
	switch (order)
	{
	case ColourSlots::ExtendedOrder::NewestFirst:
		return "newest";
	case ColourSlots::ExtendedOrder::ByName:
		return "name";
	case ColourSlots::ExtendedOrder::Custom:
		return "custom";
	default:
		return "oldest";
	}
}

ColourSlots::ExtendedOrder ParseOrder(const std::string& value)
{
	if (_stricmp(value.c_str(), "newest") == 0)
		return ColourSlots::ExtendedOrder::NewestFirst;

	if (_stricmp(value.c_str(), "name") == 0)
		return ColourSlots::ExtendedOrder::ByName;

	if (_stricmp(value.c_str(), "custom") == 0)
		return ColourSlots::ExtendedOrder::Custom;

	return ColourSlots::ExtendedOrder::OldestFirst;
}

std::vector<std::string> SplitOrder(const std::string& raw)
{
	std::vector<std::string> out;
	size_t start = 0;

	while (start <= raw.size())
	{
		const size_t sep = raw.find(kOrderSeparator, start);
		const std::string item = sep == std::string::npos
			? raw.substr(start) : raw.substr(start, sep - start);

		if (!item.empty())
			out.push_back(item);

		if (sep == std::string::npos)
			break;

		start = sep + 1;
	}

	return out;
}

std::string JoinOrder(const std::vector<std::string>& files)
{
	std::string out;

	for (size_t i = 0; i < files.size(); ++i)
	{
		if (files[i].empty())
			continue;

		if (!out.empty())
			out.push_back(kOrderSeparator);

		out += files[i];
	}

	return out;
}

using Entries = std::unordered_map<std::string, std::string>;

std::string Lowered(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(),
		[](char letter) { return static_cast<char>(tolower(static_cast<unsigned char>(letter))); });

	return text;
}

std::string Trimmed(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t");

	if (first == std::string::npos)
		return std::string();

	return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

std::string Unquoted(const std::string& value)
{
	const bool quoted = value.size() >= 2 && value.front() == '"' && value.back() == '"';

	return quoted ? value.substr(1, value.size() - 2) : value;
}

std::vector<char> ReadSection(const std::string& path)
{
	std::vector<char> block(kSectionBytes);

	for (;;)
	{
		const DWORD size = static_cast<DWORD>(block.size());
		const DWORD copied = Ini::GetSection(kSection, block.data(), size, path.c_str());

		if (copied < size - kSectionTruncatedMargin)
			return block;

		block.resize(block.size() * 2);
	}
}

Entries ParseSection(const std::vector<char>& block)
{
	Entries entries;

	for (const char* line = block.data(); *line != '\0'; line += strlen(line) + 1)
	{
		const char* const equals = strchr(line, '=');

		if (equals == nullptr || line[0] == ';')
			continue;

		const std::string key = Lowered(Trimmed(std::string(line, equals)));
		entries.emplace(key, Unquoted(Trimmed(equals + 1)));
	}

	return entries;
}

void LoadShelves()
{
	const Entries entries = ParseSection(ReadSection(Settings::GetIniPath()));

	const Entries::const_iterator orderFound = entries.find(Lowered(kOrderKey));
	g_extendedOrder = orderFound == entries.end()
		? ColourSlots::ExtendedOrder::OldestFirst
		: ParseOrder(orderFound->second);
	g_orderLoaded = true;

	for (int chara = 0; chara < ColourSlots::kCharacters; ++chara)
	{
		Shelf& shelf = g_shelves[chara];

		for (int colour = 0; colour < ColourSlots::kGameColours; ++colour)
		{
			char key[64] = {};
			KeyFor(chara, colour, key, sizeof(key));

			const Entries::const_iterator found = entries.find(Lowered(key));
			shelf.files[colour] = found == entries.end() ? std::string() : found->second;
		}

		char orderKey[64] = {};
		OrderKeyFor(chara, orderKey, sizeof(orderKey));
		const Entries::const_iterator customFound = entries.find(Lowered(orderKey));
		shelf.customOrder = customFound == entries.end()
			? std::vector<std::string>()
			: SplitOrder(customFound->second);

		shelf.loaded = true;
	}
}

Shelf& ShelfFor(int chara)
{
	Shelf& shelf = g_shelves[chara];

	if (!shelf.loaded)
		LoadShelves();

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

int RankInCustom(const Shelf& shelf, const char* file)
{
	if (!IsFile(file))
		return -1;

	for (size_t i = 0; i < shelf.customOrder.size(); ++i)
	{
		if (_stricmp(shelf.customOrder[i].c_str(), file) == 0)
			return static_cast<int>(i);
	}

	return -1;
}

int OrderFiles(int chara, int* order)
{
	const int count = (std::min)(PaletteLibrary::GetCount(chara), static_cast<int>(PaletteLibrary::kMaxFiles));

	for (int i = 0; i < count; ++i)
		order[i] = i;

	const Shelf& shelf = ShelfFor(chara);
	const ColourSlots::ExtendedOrder mode = g_extendedOrder;

	std::stable_sort(order, order + count, [chara, &shelf, mode](int a, int b)
	{
		const char* const nameA = PaletteLibrary::GetName(chara, a);
		const char* const nameB = PaletteLibrary::GetName(chara, b);

		if (mode == ColourSlots::ExtendedOrder::Custom)
		{
			const int rankA = RankInCustom(shelf, nameA);
			const int rankB = RankInCustom(shelf, nameB);

			if (rankA != rankB)
			{
				if (rankA < 0)
					return false;
				if (rankB < 0)
					return true;
				return rankA < rankB;
			}
		}

		if (mode == ColourSlots::ExtendedOrder::ByName)
			return _stricmp(nameA, nameB) < 0;

		const uint64_t timeA = PaletteLibrary::GetCreated(chara, a);
		const uint64_t timeB = PaletteLibrary::GetCreated(chara, b);

		if (timeA != timeB)
		{
			return mode == ColourSlots::ExtendedOrder::NewestFirst
				? timeA > timeB
				: timeA < timeB;
		}

		return _stricmp(nameA, nameB) < 0;
	});

	return count;
}

template <typename Visit>
int WalkExtended(int chara, Visit visit)
{
	if (!IsCharacter(chara))
		return ColourSlots::kNoExtended;

	int order[PaletteLibrary::kMaxFiles] = {};
	const int count = OrderFiles(chara, order);
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

void ColourSlots::Label(int chara, int colour, char* out, size_t size)
{
	ColorCustomize::ColourLabel(colour, out, size);

	const char* const holder = BoundFile(chara, colour);

	if (holder[0] == '\0')
		return;

	const size_t used = strlen(out);
	sprintf_s(out + used, size - used, " - %s", holder);
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

ColourSlots::ExtendedOrder ColourSlots::GetExtendedOrder()
{
	if (!g_orderLoaded)
		LoadShelves();

	return g_extendedOrder;
}

void ColourSlots::SetExtendedOrder(ExtendedOrder order)
{
	if (!g_orderLoaded)
		LoadShelves();

	g_extendedOrder = order;
	Settings::SaveString(kSection, kOrderKey, OrderToString(order));
}

int ColourSlots::CustomOrderRank(int chara, const char* file)
{
	if (!IsCharacter(chara) || !IsFile(file))
		return -1;

	return RankInCustom(ShelfFor(chara), file);
}

void ColourSlots::SetCustomOrder(int chara, const std::vector<std::string>& files)
{
	if (!IsCharacter(chara))
		return;

	Shelf& shelf = ShelfFor(chara);
	shelf.customOrder = files;

	char key[64] = {};
	OrderKeyFor(chara, key, sizeof(key));

	const std::string joined = JoinOrder(files);
	Settings::SaveString(kSection, key, joined.empty() ? nullptr : joined.c_str());
}

void ColourSlots::Reload()
{
	g_orderLoaded = false;

	for (Shelf& shelf : g_shelves)
		shelf.loaded = false;
}
