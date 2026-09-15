#include "Game/ModPacks.h"

#include "Core/FileIndex.h"
#include "Core/Settings.h"
#include "Core/ZipArchive.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr const char* kFolder = "Packs";
constexpr const char* kDescription = "mod.ini";
constexpr const char* kSection = "Mods";
constexpr const char* kOrderKey = "Order";
constexpr const char* kOffKey = "Off";
constexpr char kSeparator = '|';
constexpr int kListBytes = 4096;
constexpr int kNoStage = -1;

struct Entry
{
	ModPacks::Pack info;
	FileIndex files;
};

std::vector<Entry> g_packs;
char g_status[160] = "not scanned";

std::string Read(const std::string& path, const char* key, const char* fallback)
{
	char value[192] = {};

	GetPrivateProfileStringA("Mod", key, fallback, value, sizeof(value), path.c_str());

	return value;
}

std::vector<std::string> Split(const std::string& text)
{
	std::vector<std::string> out;
	std::string current;

	for (char c : text)
	{
		if (c != kSeparator)
		{
			current.push_back(c);
			continue;
		}

		if (!current.empty())
			out.push_back(current);

		current.clear();
	}

	if (!current.empty())
		out.push_back(current);

	return out;
}

std::vector<std::string> Stored(const char* key)
{
	std::vector<char> value(kListBytes, 0);

	GetPrivateProfileStringA(kSection, key, "", value.data(), kListBytes,
		Settings::GetIniPath().c_str());

	return Split(value.data());
}

std::string LeafOf(const std::string& path)
{
	const size_t slash = path.find_last_of("\\/");
	const std::string leaf = slash == std::string::npos ? path : path.substr(slash + 1);
	const size_t dot = leaf.rfind('.');

	return dot == std::string::npos ? leaf : leaf.substr(0, dot);
}

std::string SharedFolder(const std::vector<std::string>& names)
{
	std::string root;

	for (const std::string& name : names)
	{
		const size_t slash = name.find_first_of("\\/");

		if (slash == std::string::npos || slash == 0)
			return std::string();

		const std::string here = name.substr(0, slash);

		if (root.empty())
			root = here;
		else if (_stricmp(root.c_str(), here.c_str()) != 0)
			return std::string();
	}

	return root;
}

bool Listed(const std::vector<std::string>& list, const std::string& id)
{
	return std::find(list.begin(), list.end(), id) != list.end();
}

int PlaceOf(const std::vector<std::string>& order, const std::string& id)
{
	const auto found = std::find(order.begin(), order.end(), id);

	if (found == order.end())
		return static_cast<int>(order.size());

	return static_cast<int>(found - order.begin());
}

int StageIn(const FileIndex& files)
{
	const std::string prefix = "bg\\bg";

	for (const FileIndex::Map::value_type& entry : files.Entries())
	{
		if (entry.first.compare(0, prefix.size(), prefix) != 0)
			continue;

		const int number = atoi(entry.first.c_str() + prefix.size());

		if (number > 0)
			return number;
	}

	return kNoStage;
}

void Weigh()
{
	FileIndex seen;

	for (Entry& pack : g_packs)
	{
		pack.info.beaten = 0;
		pack.info.beatenBy.clear();

		if (!pack.info.enabled)
			continue;

		for (const FileIndex::Map::value_type& entry : pack.files.Entries())
		{
			const std::string* const already = seen.Find(entry.first);

			if (already != nullptr)
			{
				++pack.info.beaten;

				if (pack.info.beatenBy.empty())
					pack.info.beatenBy = *already;

				continue;
			}

			seen.Add(entry.first, pack.info.name);
		}

		if (pack.info.beaten == 0)
			continue;

		LOG("ModPacks: %d file(s) of %s are already answered by %s", pack.info.beaten,
			pack.info.name.c_str(), pack.info.beatenBy.c_str());
	}
}

void Save()
{
	std::string order;
	std::string off;

	for (const Entry& pack : g_packs)
	{
		if (!order.empty())
			order.push_back(kSeparator);

		order += pack.info.id;

		if (pack.info.enabled)
			continue;

		if (!off.empty())
			off.push_back(kSeparator);

		off += pack.info.id;
	}

	Settings::SaveString(kSection, kOrderKey, order.c_str());
	Settings::SaveString(kSection, kOffKey, off.c_str());
}

void Take(const std::string& folder, const std::string& id, const std::vector<std::string>& off)
{
	Entry pack;

	pack.info.id = id;
	pack.info.path = folder;
	pack.info.enabled = !Listed(off, id);

	const std::string description = folder + "\\" + kDescription;

	pack.info.described = GetFileAttributesA(description.c_str()) != INVALID_FILE_ATTRIBUTES;
	pack.info.name = pack.info.described ? Read(description, "Name", id.c_str()) : id;
	pack.info.author = pack.info.described ? Read(description, "Author", "") : std::string();
	pack.info.version = pack.info.described ? Read(description, "Version", "") : std::string();
	pack.info.note = pack.info.described ? Read(description, "Note", "") : std::string();

	pack.files.Walk(folder);
	pack.files.Remove(FileIndex::Key(kDescription));

	pack.info.files = pack.files.Count();
	pack.info.stage = StageIn(pack.files);

	g_packs.push_back(pack);
}

}

void ModPacks::Scan()
{
	const std::string root = Root();

	g_packs.clear();

	if (GetFileAttributesA(root.c_str()) == INVALID_FILE_ATTRIBUTES)
		CreateDirectoryA(root.c_str(), nullptr);

	const std::vector<std::string> off = Stored(kOffKey);
	const std::vector<std::string> order = Stored(kOrderKey);

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (found.cFileName[0] == '.' ||
				(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				continue;
			}

			Take(root + "\\" + found.cFileName, found.cFileName, off);
		}
		while (FindNextFileA(search, &found) != 0);

		FindClose(search);
	}

	std::stable_sort(g_packs.begin(), g_packs.end(),
		[&order](const Entry& a, const Entry& b)
		{
			return PlaceOf(order, a.info.id) < PlaceOf(order, b.info.id);
		});

	Weigh();

	sprintf_s(g_status, "%d mod(s), %d on", Count(), EnabledCount());
	LOG("ModPacks: %s, from %s", g_status, root.c_str());

	Save();
}

int ModPacks::Count()
{
	return static_cast<int>(g_packs.size());
}

const ModPacks::Pack* ModPacks::At(int index)
{
	if (index < 0 || index >= Count())
		return nullptr;

	return &g_packs[index].info;
}

int ModPacks::EnabledCount()
{
	int on = 0;

	for (const Entry& pack : g_packs)
	{
		if (pack.info.enabled)
			++on;
	}

	return on;
}

int ModPacks::FileCount()
{
	int files = 0;

	for (const Entry& pack : g_packs)
	{
		if (pack.info.enabled)
			files += pack.info.files;
	}

	return files;
}

void ModPacks::SetEnabled(int index, bool enabled)
{
	if (index < 0 || index >= Count() || g_packs[index].info.enabled == enabled)
		return;

	g_packs[index].info.enabled = enabled;

	LOG("ModPacks: %s is %s", g_packs[index].info.name.c_str(), enabled ? "on" : "off");

	Weigh();
	Save();
}

bool ModPacks::Move(int index, int delta)
{
	const int to = index + delta;

	if (index < 0 || index >= Count() || to < 0 || to >= Count())
		return false;

	std::swap(g_packs[index], g_packs[to]);

	Weigh();
	Save();

	return true;
}

void ModPacks::Layer(FileIndex& into)
{
	for (size_t i = g_packs.size(); i-- > 0;)
	{
		if (!g_packs[i].info.enabled)
			continue;

		for (const FileIndex::Map::value_type& entry : g_packs[i].files.Entries())
			into.Add(entry.first, entry.second);
	}
}

bool ModPacks::Install(const std::string& zip, char* status, int statusSize)
{
	std::vector<std::string> names;

	if (!ZipArchive::List(zip, names) || names.empty())
	{
		strncpy_s(status, statusSize, "that zip could not be read", _TRUNCATE);
		return false;
	}

	const std::string inside = SharedFolder(names);
	std::string folder = Root();

	if (inside.empty())
		folder += "\\" + LeafOf(zip);

	int files = 0;
	char note[192] = {};

	if (!ZipArchive::Extract(zip, folder, files, note, sizeof(note)))
	{
		sprintf_s(status, statusSize, "the zip could not be unpacked: %s", note);
		return false;
	}

	Scan();

	sprintf_s(status, statusSize, "%s installed, %d file(s)",
		inside.empty() ? LeafOf(zip).c_str() : inside.c_str(), files);

	LOG("ModPacks: %s", status);
	return true;
}

int ModPacks::StageOwner(int number, int except)
{
	if (number == kNoStage)
		return -1;

	for (int i = 0; i < Count(); ++i)
	{
		if (i == except || !g_packs[i].info.enabled)
			continue;

		if (g_packs[i].info.stage == number)
			return i;
	}

	return -1;
}

std::string ModPacks::Root()
{
	return GetModRootPath(kFolder);
}

const char* ModPacks::StatusText()
{
	return g_status;
}
