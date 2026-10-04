#include "Game/Customize/PortraitLayer.h"

#include "Core/Config/Settings.h"
#include "Core/FileIndex.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Customize/PortraitPicks.h"
#include "Game/Files/ModFiles.h"

#include <Windows.h>

#include <algorithm>
#include <mutex>
#include <set>

namespace {

constexpr const char* kFolder = "Portraits";
constexpr const char* kSection = "Portraits";
constexpr const char* kKey = "Old";
constexpr size_t kListBytes = 512;

std::mutex g_lock;
std::set<int> g_available;
std::set<int> g_worn;
bool g_loaded = false;
bool g_everyone = false;

void LoadLocked()
{
	if (g_loaded)
		return;

	g_loaded = true;

	char buffer[kListBytes] = {};
	GetPrivateProfileStringA(kSection, kKey, "", buffer, sizeof(buffer), Settings::GetIniPath().c_str());

	g_everyone = buffer[0] == '\0';

	if (g_everyone)
		return;

	const std::vector<int> picked = PortraitPicks::Parse(buffer);
	g_worn = std::set<int>(picked.begin(), picked.end());
}

void SaveLocked()
{
	g_everyone = false;

	Settings::SaveString(kSection, kKey,
		PortraitPicks::Joined(std::vector<int>(g_worn.begin(), g_worn.end())).c_str());
}

}

std::string PortraitLayer::Folder()
{
	return GetModRootPath(kFolder);
}

void PortraitLayer::Layer(FileIndex& into)
{
	FileIndex own;
	own.Walk(Folder());

	std::lock_guard<std::mutex> hold(g_lock);
	g_available.clear();

	for (const FileIndex::Map::value_type& entry : own.Entries())
	{
		const int chara = PortraitPicks::CharaOf(entry.first);

		if (chara >= 0)
			g_available.insert(chara);
	}

	const bool fresh = !g_loaded;
	LoadLocked();

	if (g_everyone)
		g_worn = g_available;

	if (fresh && !g_available.empty())
		LOG("PortraitLayer: %d character(s) have their old art, %d wear it", static_cast<int>(g_available.size()),
			static_cast<int>(g_worn.size()));

	for (const FileIndex::Map::value_type& entry : own.Entries())
	{
		if (g_worn.count(PortraitPicks::CharaOf(entry.first)) != 0)
			into.Add(entry.first, entry.second);
	}
}

std::vector<int> PortraitLayer::Available()
{
	std::lock_guard<std::mutex> hold(g_lock);

	return std::vector<int>(g_available.begin(), g_available.end());
}

bool PortraitLayer::IsWorn(int chara)
{
	std::lock_guard<std::mutex> hold(g_lock);
	LoadLocked();

	return (g_everyone ? g_available : g_worn).count(chara) != 0;
}

void PortraitLayer::Wear(int chara, bool old)
{
	{
		std::lock_guard<std::mutex> hold(g_lock);
		LoadLocked();

		if (g_everyone)
			g_worn = g_available;

		if ((g_worn.count(chara) != 0) == old)
			return;

		if (old)
			g_worn.insert(chara);
		else
			g_worn.erase(chara);

		SaveLocked();
	}

	ModFiles::Rescan();
}

void PortraitLayer::WearAll(bool old)
{
	{
		std::lock_guard<std::mutex> hold(g_lock);
		LoadLocked();

		g_worn = old ? g_available : std::set<int>();
		SaveLocked();
	}

	ModFiles::Rescan();
}
