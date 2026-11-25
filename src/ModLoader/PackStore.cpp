#include "ModLoader/PackStore.h"

#include "Core/FileSystem.h"
#include "Core/Formats/ZipArchive.h"
#include "Core/Text/TextList.h"
#include "ModLoader/LoaderConfig.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace {

std::vector<PackStore::Pack> g_packs;

bool IsDirectory(const std::string& path)
{
	const DWORD attr = GetFileAttributesA(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool EndsWithNoCase(const std::string& text, const char* tail)
{
	const size_t len = strlen(tail);
	return text.size() >= len && _stricmp(text.c_str() + text.size() - len, tail) == 0;
}

std::string LeafOf(const std::string& path)
{
	std::string clean = path;

	while (!clean.empty() && (clean.back() == '\\' || clean.back() == '/'))
		clean.pop_back();

	const size_t slash = clean.find_last_of("\\/");
	return slash == std::string::npos ? clean : clean.substr(slash + 1);
}

std::string StemOf(const std::string& path)
{
	const std::string leaf = LeafOf(path);
	const size_t dot = leaf.rfind('.');
	return dot == std::string::npos ? leaf : leaf.substr(0, dot);
}

bool Listed(const std::vector<std::string>& items, const std::string& id)
{
	return std::any_of(items.begin(), items.end(),
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

void SaveOrder(const std::string& gameDir)
{
	std::vector<std::string> order;
	std::vector<std::string> off;

	for (const PackStore::Pack& pack : g_packs)
	{
		order.push_back(pack.id);

		if (!pack.enabled)
			off.push_back(pack.id);
	}

	const std::string ini = LoaderConfig::ModulesIni(gameDir);
	LoaderConfig::WriteList(ini, "Mods", "Order", order);
	LoaderConfig::WriteList(ini, "Mods", "Off", off);
}

std::string Trim(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t\r\n");

	if (first == std::string::npos)
		return std::string();

	const size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

void ParseModIniText(const std::string& text, PackStore::Pack& pack)
{
	size_t cursor = 0;
	bool inMod = true;

	while (cursor < text.size())
	{
		const size_t end = text.find('\n', cursor);
		const std::string line = Trim(end == std::string::npos ? text.substr(cursor) : text.substr(cursor, end - cursor));
		cursor = end == std::string::npos ? text.size() : end + 1;

		if (line.empty() || line[0] == ';' || line[0] == '#')
			continue;

		if (line.front() == '[' && line.back() == ']')
		{
			inMod = TextList::SameNoCase(Trim(line.substr(1, line.size() - 2)), "Mod");
			continue;
		}

		if (!inMod)
			continue;

		const size_t eq = line.find('=');

		if (eq == std::string::npos)
			continue;

		const std::string k = Trim(line.substr(0, eq));
		const std::string v = Trim(line.substr(eq + 1));

		if (TextList::SameNoCase(k, "Name") && !v.empty())
			pack.name = v;
		else if (TextList::SameNoCase(k, "Author"))
			pack.author = v;
		else if (TextList::SameNoCase(k, "Version"))
			pack.version = v;
	}
}

void ReadZipModMeta(const std::string& zipPath, PackStore::Pack& pack)
{
	ZipArchive::Source src;

	if (!src.Open(zipPath))
		return;

	for (int i = 0; i < src.Count(); ++i)
	{
		const std::string& name = src.Name(i);
		const std::string leaf = LeafOf(name);

		if (!TextList::SameNoCase(leaf, "mod.ini"))
			continue;

		std::vector<uint8_t> bytes;

		if (src.Read(i, bytes) && !bytes.empty())
			ParseModIniText(std::string(bytes.begin(), bytes.end()), pack);

		return;
	}
}

void ScanFolder(const std::string& root)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.')
			continue;

		const bool isDir = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		const bool isZip = !isDir && EndsWithNoCase(found.cFileName, ".zip");

		if (!isDir && !isZip)
			continue;

		const std::string id = isZip ? StemOf(found.cFileName) : std::string(found.cFileName);
		const bool duplicate = std::any_of(g_packs.begin(), g_packs.end(),
			[&id](const PackStore::Pack& p) { return TextList::SameNoCase(p.id, id); });

		if (duplicate)
			continue;

		PackStore::Pack pack;
		pack.id = id;
		pack.name = id;
		pack.folder = root + "\\" + found.cFileName;
		pack.isZip = isZip;

		if (isZip)
		{
			ReadZipModMeta(pack.folder, pack);
		}
		else
		{
			const std::string modIni = pack.folder + "\\mod.ini";
			char nameBuf[128] = {};
			char authorBuf[128] = {};
			char verBuf[64] = {};

			GetPrivateProfileStringA("Mod", "Name", id.c_str(), nameBuf, sizeof(nameBuf), modIni.c_str());
			GetPrivateProfileStringA("Mod", "Author", "", authorBuf, sizeof(authorBuf), modIni.c_str());
			GetPrivateProfileStringA("Mod", "Version", "", verBuf, sizeof(verBuf), modIni.c_str());

			pack.name = nameBuf;
			pack.author = authorBuf;
			pack.version = verBuf;
		}

		g_packs.push_back(std::move(pack));
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

}

std::string PackStore::Root(const std::string& gameDir)
{
	return LoaderConfig::ModRoot(gameDir) + "\\Packs";
}

std::string PackStore::GameModsRoot(const std::string& gameDir)
{
	std::string dir = gameDir;

	while (!dir.empty() && (dir.back() == '\\' || dir.back() == '/'))
		dir.pop_back();

	return dir + "\\mods";
}

void PackStore::Refresh(const std::string& gameDir)
{
	g_packs.clear();

	if (!LoaderConfig::IsValidGameDir(gameDir))
		return;

	const std::string packsRoot = Root(gameDir);
	const std::string modsRoot = GameModsRoot(gameDir);
	CreateDirectoryA(LoaderConfig::ModRoot(gameDir).c_str(), nullptr);
	CreateDirectoryA(packsRoot.c_str(), nullptr);
	CreateDirectoryA(modsRoot.c_str(), nullptr);

	ScanFolder(packsRoot);
	ScanFolder(modsRoot);

	const std::string ini = LoaderConfig::ModulesIni(gameDir);
	const std::vector<std::string> order = LoaderConfig::ReadList(ini, "Mods", "Order");
	const std::vector<std::string> off = LoaderConfig::ReadList(ini, "Mods", "Off");

	for (Pack& pack : g_packs)
		pack.enabled = !Listed(off, pack.id);

	std::stable_sort(g_packs.begin(), g_packs.end(),
		[&order](const Pack& a, const Pack& b) { return PlaceOf(order, a.id) < PlaceOf(order, b.id); });
}

const std::vector<PackStore::Pack>& PackStore::Items()
{
	return g_packs;
}

void PackStore::SetEnabled(const std::string& gameDir, int index, bool enabled)
{
	if (index < 0 || index >= static_cast<int>(g_packs.size()))
		return;

	g_packs[index].enabled = enabled;
	SaveOrder(gameDir);
}

bool PackStore::Move(const std::string& gameDir, int index, int delta)
{
	const int target = index + delta;

	if (index < 0 || target < 0 || index >= static_cast<int>(g_packs.size()) ||
		target >= static_cast<int>(g_packs.size()))
	{
		return false;
	}

	std::swap(g_packs[index], g_packs[target]);
	SaveOrder(gameDir);
	return true;
}

bool PackStore::InstallFromPath(const std::string& gameDir, const std::string& source, std::string& outMessage)
{
	const std::string modsRoot = GameModsRoot(gameDir);
	CreateDirectoryA(modsRoot.c_str(), nullptr);

	if (IsDirectory(source))
	{
		const std::string target = modsRoot + "\\" + LeafOf(source);
		int files = 0;

		if (!CopyDirectoryTree(source, target, files))
		{
			outMessage = "Could not copy mod folder.";
			return false;
		}

		outMessage = "Installed mod folder " + LeafOf(source) + " into mods\\ (" + std::to_string(files) + " file(s)).";
		Refresh(gameDir);
		return true;
	}

	std::vector<std::string> names;

	if (!ZipArchive::List(source, names) || names.empty())
	{
		outMessage = "Could not read zip archive.";
		return false;
	}

	const std::string destZip = modsRoot + "\\" + LeafOf(source);

	if (!CopyFileA(source.c_str(), destZip.c_str(), FALSE))
	{
		outMessage = "Could not copy zip mod into mods\\.";
		return false;
	}

	outMessage = "Installed zipped mod " + LeafOf(source) + " into mods\\ (" + std::to_string(names.size()) + " entries).";
	Refresh(gameDir);
	return true;
}

bool PackStore::Remove(const std::string& gameDir, int index, std::string& outMessage)
{
	if (index < 0 || index >= static_cast<int>(g_packs.size()))
		return false;

	const std::string name = g_packs[index].name;

	if (!MoveToRecycleBin(g_packs[index].folder))
	{
		outMessage = "Could not remove " + name + ".";
		return false;
	}

	g_packs.erase(g_packs.begin() + index);
	SaveOrder(gameDir);
	outMessage = "Moved " + name + " to Recycle Bin.";
	return true;
}
