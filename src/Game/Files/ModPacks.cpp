#include "Game/Files/ModPacks.h"

#include "Core/BackgroundJob.h"
#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/FileIndex.h"
#include "Core/FileSystem.h"
#include "Core/Formats/ZipArchive.h"
#include "Core/Text/TextList.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Audio/BgmOverride.h"
#include "Game/Files/ModLayout.h"
#include "Game/Stages/BgListOverride.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace {

constexpr const char* kFolder = "Packs";
constexpr const char* kDescription = "mod.ini";
constexpr const char* kSection = "Mods";
constexpr const char* kOrderKey = "Order";
constexpr const char* kOffKey = "Off";
constexpr char kSeparator = '|';
constexpr const char* kSeparators = "|";
constexpr int kListBytes = 32768;
constexpr const char* kStageNameKey = "Name";
constexpr const char* kStageCardKey = "Card";
constexpr int kNoCard = -1;
constexpr int kNoIndex = -1;

struct Entry
{
	ModPacks::Pack info;
	FileIndex files;
};

std::vector<Entry> g_packs;
char g_status[160] = "not scanned";
volatile long g_revision = 0;

BackgroundJob g_scanJob("ModPacks scan");
std::mutex g_scannedLock;
std::vector<Entry> g_scanned;
bool g_scanAgain = false;

BackgroundJob g_installJob("ModPacks install");

std::vector<std::string> Stored(const char* key)
{
	std::vector<char> value(kListBytes, 0);

	Ini::GetString(kSection, key, "", value.data(), kListBytes, Settings::GetIniPath().c_str());

	return TextList::Split(value.data(), kSeparators);
}

std::string LeafOf(const std::string& path)
{
	const size_t slash = path.find_last_of("\\/");

	return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string StemOf(const std::string& path)
{
	const std::string leaf = LeafOf(path);
	const size_t dot = leaf.rfind('.');

	return dot == std::string::npos ? leaf : leaf.substr(0, dot);
}

bool IsFolder(const std::string& path)
{
	const DWORD attributes = GetFileAttributesA(path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
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
		else if (!TextList::SameNoCase(root, here))
			return std::string();
	}

	return root;
}

bool Listed(const std::vector<std::string>& list, const std::string& id)
{
	return std::any_of(list.begin(), list.end(),
		[&id](const std::string& item) { return TextList::SameNoCase(item, id); });
}

int PlaceOf(const std::vector<std::string>& order, const std::string& id)
{
	for (size_t i = 0; i < order.size(); ++i)
	{
		if (TextList::SameNoCase(order[i], id))
			return static_cast<int>(i);
	}

	return static_cast<int>(order.size());
}

bool IsServedTable(const std::string& key)
{
	return BgListOverride::IsTable(key) || BgmOverride::IsTable(key);
}

std::vector<std::string> KeysOf(const FileIndex& files)
{
	std::vector<std::string> keys;
	keys.reserve(files.Entries().size());

	for (const FileIndex::Map::value_type& entry : files.Entries())
		keys.push_back(entry.first);

	return keys;
}

int CountGameplay(const std::vector<std::string>& keys)
{
	return static_cast<int>(std::count_if(keys.begin(), keys.end(), &ModLayout::ChangesGameplay));
}

ModManifest ReadManifest(const std::string& folder, const std::string& id)
{
	std::vector<uint8_t> bytes;

	if (!ReadWholeFile(folder + "\\" + kDescription, bytes))
		return ModManifest::Parse(std::string(), id);

	return ModManifest::Parse(std::string(bytes.begin(), bytes.end()), id);
}

Entry Take(const std::string& folder, const std::string& id)
{
	Entry pack;

	pack.info.id = id;
	pack.info.path = folder;
	pack.info.manifest = ReadManifest(folder, id);

	pack.files.Walk(folder);
	pack.files.Remove(FileIndex::Key(kDescription));

	const std::vector<std::string> keys = KeysOf(pack.files);

	pack.info.files = pack.files.Count();
	pack.info.stages = PackStages::Find(keys);
	pack.info.wrapper = ModLayout::WrapperFolder(keys);
	pack.info.gameplayFiles = CountGameplay(keys);

	return pack;
}

std::vector<Entry> Collect(const std::string& root)
{
	std::vector<Entry> packs;

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return packs;

	do
	{
		if (found.cFileName[0] == '.' || (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		packs.push_back(Take(root + "\\" + found.cFileName, found.cFileName));
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return packs;
}

void Arrange(std::vector<Entry>& packs, const std::vector<std::string>& order, const std::vector<std::string>& off)
{
	for (Entry& pack : packs)
		pack.info.enabled = !Listed(off, pack.info.id);

	std::stable_sort(packs.begin(), packs.end(),
		[&order](const Entry& a, const Entry& b) { return PlaceOf(order, a.info.id) < PlaceOf(order, b.info.id); });
}

std::vector<ModRules::Mod> Rules()
{
	std::vector<ModRules::Mod> mods;
	mods.reserve(g_packs.size());

	for (const Entry& pack : g_packs)
	{
		const ModManifest& manifest = pack.info.manifest;

		mods.push_back({ pack.info.id, manifest.id, pack.info.enabled, manifest.requirements, manifest.conflicts,
			manifest.above, manifest.below });
	}

	return mods;
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
			if (IsServedTable(entry.first))
				continue;

			const std::string* const already = seen.Find(entry.first);

			if (already == nullptr)
			{
				seen.Add(entry.first, pack.info.manifest.name);
				continue;
			}

			++pack.info.beaten;

			if (pack.info.beatenBy.empty())
				pack.info.beatenBy = *already;
		}

		if (pack.info.beaten > 0)
		{
			LOG("ModPacks: %d file(s) of %s are already answered by %s", pack.info.beaten,
				pack.info.manifest.name.c_str(), pack.info.beatenBy.c_str());
		}
	}
}

void CheckRules()
{
	const std::vector<std::vector<ModRules::Issue>> issues = ModRules::Check(Rules());

	for (size_t i = 0; i < g_packs.size(); ++i)
	{
		g_packs[i].info.issues = issues[i];

		if (!issues[i].empty())
			LOG_WARN("ModPacks: %s has %zu rule issue(s)", g_packs[i].info.manifest.name.c_str(), issues[i].size());
	}
}

void Refresh()
{
	Weigh();
	CheckRules();

	sprintf_s(g_status, "%d mod(s), %d on", ModPacks::Count(), ModPacks::EnabledCount());
	InterlockedIncrement(&g_revision);
}

void Save()
{
	std::vector<std::string> order;
	std::vector<std::string> off;
	ModPacks::Snapshot(order, off);

	Settings::SaveString(kSection, kOrderKey, TextList::Join(order, kSeparator).c_str());
	Settings::SaveString(kSection, kOffKey, TextList::Join(off, kSeparator).c_str());
}

void Changed()
{
	Refresh();
	Save();
}

bool Valid(int index)
{
	return index >= 0 && index < ModPacks::Count();
}

bool InstallZip(const std::string& zip, std::string& message)
{
	std::vector<std::string> names;

	if (!ZipArchive::List(zip, names) || names.empty())
	{
		message = "that zip could not be read";
		return false;
	}

	const std::string inside = SharedFolder(names);
	const std::string name = inside.empty() ? StemOf(zip) : inside;
	const std::string folder = inside.empty() ? ModPacks::Root() + "\\" + name : ModPacks::Root();

	int files = 0;
	char note[192] = {};

	if (!ZipArchive::Extract(zip, folder, files, note, sizeof(note)))
	{
		message = std::string("the zip could not be unpacked: ") + note;
		return false;
	}

	message = name + " installed, " + std::to_string(files) + " file(s)";
	return true;
}

bool InstallFolder(const std::string& source, std::string& message)
{
	const std::string name = LeafOf(source);
	const std::string target = ModPacks::Root() + "\\" + name;

	if (SameFolder(source, target))
	{
		message = name + " is already in the mods folder";
		return false;
	}

	int files = 0;

	if (!CopyDirectoryTree(source, target, files))
	{
		message = "the folder " + name + " could not be copied";
		return false;
	}

	message = name + " installed, " + std::to_string(files) + " file(s)";
	return true;
}

bool RunInstall(const std::string& source)
{
	std::string message;
	const bool installed = IsFolder(source) ? InstallFolder(source, message) : InstallZip(source, message);

	g_installJob.SetStatus(message.c_str());
	return installed;
}

bool MoveChildrenUp(const std::string& inner, const std::string& outer, int& moved, int& kept)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((inner + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0)
			continue;

		const std::string from = inner + "\\" + found.cFileName;
		const std::string to = outer + "\\" + found.cFileName;

		if (!Exists(to) && MoveFileExA(from.c_str(), to.c_str(), 0) != FALSE)
			++moved;
		else
			++kept;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return true;
}

}

void ModPacks::Scan()
{
	const std::string root = Root();

	if (!Exists(root))
		CreateDirectoryA(root.c_str(), nullptr);

	std::vector<Entry> packs = Collect(root);
	Arrange(packs, Stored(kOrderKey), Stored(kOffKey));
	g_packs.swap(packs);

	Changed();
	LOG("ModPacks: %s, from %s", g_status, root.c_str());
}

void ModPacks::RequestScan()
{
	if (g_scanJob.IsBusy())
	{
		g_scanAgain = true;
		return;
	}

	g_scanAgain = false;
	const std::string root = Root();

	g_scanJob.Start([root]()
	{
		std::vector<Entry> packs = Collect(root);

		std::lock_guard<std::mutex> lock(g_scannedLock);
		g_scanned.swap(packs);
		return true;
	});
}

bool ModPacks::AdoptScan()
{
	if (g_installJob.ConsumeFinished())
		RequestScan();

	if (!g_scanJob.ConsumeFinished())
	{
		if (g_scanAgain && !g_scanJob.IsBusy())
			RequestScan();

		return false;
	}

	std::vector<Entry> packs;

	{
		std::lock_guard<std::mutex> lock(g_scannedLock);
		packs.swap(g_scanned);
	}

	std::vector<std::string> order;
	std::vector<std::string> off;
	Snapshot(order, off);

	Arrange(packs, order, off);
	g_packs.swap(packs);

	Changed();
	LOG("ModPacks: %s after a background scan", g_status);

	if (g_scanAgain)
		RequestScan();

	return true;
}

bool ModPacks::IsScanning()
{
	return g_scanJob.IsBusy();
}

long ModPacks::Revision()
{
	return InterlockedCompareExchange(&g_revision, 0, 0);
}

int ModPacks::Count()
{
	return static_cast<int>(g_packs.size());
}

const ModPacks::Pack* ModPacks::At(int index)
{
	if (!Valid(index))
		return nullptr;

	return &g_packs[index].info;
}

int ModPacks::EnabledCount()
{
	return static_cast<int>(std::count_if(g_packs.begin(), g_packs.end(),
		[](const Entry& pack) { return pack.info.enabled; }));
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

int ModPacks::IssueCount()
{
	int issues = 0;

	for (const Entry& pack : g_packs)
		issues += static_cast<int>(pack.info.issues.size());

	return issues;
}

int ModPacks::SetEnabled(int index, bool enabled)
{
	if (!Valid(index) || g_packs[index].info.enabled == enabled)
		return 0;

	g_packs[index].info.enabled = enabled;
	LOG("ModPacks: %s is %s", g_packs[index].info.manifest.name.c_str(), enabled ? "on" : "off");

	const std::vector<int> needed = enabled ? ModRules::NeededBy(Rules(), index) : std::vector<int>();

	for (const int other : needed)
	{
		g_packs[other].info.enabled = true;
		LOG("ModPacks: %s is on, %s needs it", g_packs[other].info.manifest.name.c_str(),
			g_packs[index].info.manifest.name.c_str());
	}

	Changed();
	return static_cast<int>(needed.size());
}

void ModPacks::SetAllEnabled(bool enabled)
{
	for (Entry& pack : g_packs)
		pack.info.enabled = enabled;

	LOG("ModPacks: every mod is %s", enabled ? "on" : "off");
	Changed();
}

bool ModPacks::Move(int index, int delta)
{
	return MoveTo(index, index + delta);
}

bool ModPacks::MoveTo(int index, int to)
{
	if (!Valid(index) || !Valid(to) || index == to)
		return false;

	if (index < to)
		std::rotate(g_packs.begin() + index, g_packs.begin() + index + 1, g_packs.begin() + to + 1);
	else
		std::rotate(g_packs.begin() + to, g_packs.begin() + index, g_packs.begin() + index + 1);

	Changed();
	return true;
}

bool ModPacks::SortByRules()
{
	std::vector<int> order;

	if (!ModRules::Sorted(Rules(), order))
	{
		LOG_WARN("ModPacks: the Above, Below and Requires rules contradict each other, nothing was moved");
		return false;
	}

	std::vector<Entry> sorted;
	sorted.reserve(g_packs.size());

	for (const int index : order)
		sorted.push_back(std::move(g_packs[index]));

	g_packs.swap(sorted);

	LOG("ModPacks: sorted by the mods' own rules");
	Changed();
	return true;
}

void ModPacks::Snapshot(std::vector<std::string>& order, std::vector<std::string>& off)
{
	order.clear();
	off.clear();

	for (const Entry& pack : g_packs)
	{
		order.push_back(pack.info.id);

		if (!pack.info.enabled)
			off.push_back(pack.info.id);
	}
}

void ModPacks::Apply(const std::vector<std::string>& order, const std::vector<std::string>& off)
{
	Arrange(g_packs, order, off);
	Changed();
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

std::vector<std::string> ModPacks::EnabledFolders()
{
	std::vector<std::string> folders;

	for (size_t i = g_packs.size(); i-- > 0;)
	{
		if (g_packs[i].info.enabled)
			folders.push_back(g_packs[i].info.path);
	}

	return folders;
}

std::vector<ModPacks::Pack> ModPacks::EnabledTopDown()
{
	std::vector<Pack> enabled;

	for (const Entry& pack : g_packs)
	{
		if (pack.info.enabled)
			enabled.push_back(pack.info);
	}

	return enabled;
}

std::vector<std::string> ModPacks::FilesOf(int index)
{
	if (!Valid(index))
		return std::vector<std::string>();

	std::vector<std::string> keys = KeysOf(g_packs[index].files);
	std::sort(keys.begin(), keys.end());

	return keys;
}

std::vector<ModPacks::Clash> ModPacks::ClashesOf(int index)
{
	std::vector<Clash> clashes;

	if (!Valid(index) || !g_packs[index].info.enabled)
		return clashes;

	for (const std::string& key : FilesOf(index))
	{
		if (IsServedTable(key))
			continue;

		int winner = kNoIndex;
		int loser = kNoIndex;

		for (int other = 0; other < Count() && winner == kNoIndex; ++other)
		{
			if (other == index || !g_packs[other].info.enabled || !g_packs[other].files.Has(key))
				continue;

			if (other < index)
				winner = other;
			else if (loser == kNoIndex)
				loser = other;
		}

		if (winner != kNoIndex)
			clashes.push_back({ key, g_packs[winner].info.manifest.name, false });
		else if (loser != kNoIndex)
			clashes.push_back({ key, g_packs[loser].info.manifest.name, true });
	}

	return clashes;
}

bool ModPacks::BeginInstall(const std::string& source)
{
	if (g_installJob.IsBusy())
		return false;

	const std::string root = Root();

	if (!Exists(root))
		CreateDirectoryA(root.c_str(), nullptr);

	g_installJob.SetStatus(("installing " + LeafOf(source)).c_str());
	return g_installJob.Start([source]() { return RunInstall(source); });
}

bool ModPacks::IsInstalling()
{
	return g_installJob.IsBusy();
}

std::string ModPacks::InstallStatus()
{
	return g_installJob.Status();
}

bool ModPacks::Uninstall(int index, char* status, int statusSize)
{
	if (!Valid(index))
		return false;

	const std::string name = g_packs[index].info.manifest.name;

	if (!MoveToRecycleBin(g_packs[index].info.path))
	{
		sprintf_s(status, statusSize, "%s could not be moved to the Recycle Bin", name.c_str());
		LOG_WARN("ModPacks: %s", status);
		return false;
	}

	g_packs.erase(g_packs.begin() + index);
	Changed();

	sprintf_s(status, statusSize, "%s was moved to the Recycle Bin", name.c_str());
	LOG("ModPacks: %s", status);
	return true;
}

bool ModPacks::Unwrap(int index, char* status, int statusSize)
{
	if (!Valid(index) || g_packs[index].info.wrapper.empty())
		return false;

	const Pack& pack = g_packs[index].info;
	const std::string inner = pack.path + "\\" + pack.wrapper;

	int moved = 0;
	int kept = 0;

	if (!MoveChildrenUp(inner, pack.path, moved, kept))
	{
		sprintf_s(status, statusSize, "the folder %s could not be opened", pack.wrapper.c_str());
		LOG_WARN("ModPacks: %s", status);
		return false;
	}

	RemoveDirectoryA(inner.c_str());

	if (kept > 0)
		sprintf_s(status, statusSize, "%d item(s) moved up, %d left inside because the name is taken", moved, kept);
	else
		sprintf_s(status, statusSize, "%d item(s) moved up one folder", moved);

	LOG("ModPacks: %s in %s", status, pack.manifest.name.c_str());
	RequestScan();
	return true;
}

int ModPacks::StageOwner(int number, int except)
{
	for (int i = 0; i < Count(); ++i)
	{
		if (i == except || !g_packs[i].info.enabled)
			continue;

		const std::vector<PackStages::Folder>& stages = g_packs[i].info.stages;

		if (std::any_of(stages.begin(), stages.end(),
			[number](const PackStages::Folder& folder) { return folder.number == number; }))
		{
			return i;
		}
	}

	return kNoIndex;
}

void ModPacks::StageDeclared(const Pack& pack, int number, std::string& name, int& card)
{
	const std::string description = pack.path + "\\" + kDescription;
	const std::string section = PackStages::FolderName(number);

	char value[192] = {};
	Ini::GetString(section.c_str(), kStageNameKey, "", value, sizeof(value), description.c_str());

	name = value;
	card = static_cast<int>(Ini::GetInt(section.c_str(), kStageCardKey, kNoCard, description.c_str()));
}

std::string ModPacks::Root()
{
	return GetModRootPath(kFolder);
}

const char* ModPacks::StatusText()
{
	return g_status;
}
