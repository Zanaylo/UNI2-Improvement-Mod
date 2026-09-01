#include "Game/BgmCatalog.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmTable.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr int kMaxRefused = 512;

std::set<std::string> g_refused;
bool g_shuffle = false;
uint32_t g_seed = 0;

std::string IniPath()
{
	return GetModRootPath("bgm.ini");
}

uint32_t NextRandom()
{
	if (g_seed == 0)
		g_seed = GetTickCount() | 1u;

	g_seed ^= g_seed << 13;
	g_seed ^= g_seed >> 17;
	g_seed ^= g_seed << 5;

	return g_seed;
}

}

int BgmCatalog::Count()
{
	return BgmTable::kSlotCount + BgmLibrary::Count();
}

int BgmCatalog::IdAt(int index)
{
	if (index < 0 || index >= Count())
		return -1;

	if (index < BgmTable::kSlotCount)
		return index;

	return BgmLibrary::IdAt(index - BgmTable::kSlotCount);
}

bool BgmCatalog::IsListed(int id)
{
	if (!BgmLibrary::IsPlayable(id))
		return false;

	if (BgmLibrary::IsLibraryId(id))
		return true;

	return !BgmLibrary::IsMirroredSlot(id) && id != BgmLibrary::WindowSlot();
}

bool BgmCatalog::IsAllowed(int id)
{
	return g_refused.empty() || g_refused.find(BgmLibrary::RefKey(id)) == g_refused.end();
}

void BgmCatalog::SetAllowed(int id, bool allowed)
{
	if (allowed)
		g_refused.erase(BgmLibrary::RefKey(id));
	else if (static_cast<int>(g_refused.size()) < kMaxRefused)
		g_refused.insert(BgmLibrary::RefKey(id));

	Save();
}

void BgmCatalog::SetAllAllowed(bool allowed)
{
	g_refused.clear();

	if (!allowed)
	{
		for (int index = 0; index < Count() && static_cast<int>(g_refused.size()) < kMaxRefused;
			++index)
		{
			const int id = IdAt(index);

			if (IsListed(id))
				g_refused.insert(BgmLibrary::RefKey(id));
		}
	}

	Save();
}

int BgmCatalog::AllowedCount()
{
	int allowed = 0;

	for (int index = 0; index < Count(); ++index)
	{
		const int id = IdAt(index);

		if (IsListed(id) && IsAllowed(id))
			++allowed;
	}

	return allowed;
}

bool BgmCatalog::ShuffleEnabled()
{
	return g_shuffle;
}

void BgmCatalog::SetShuffleEnabled(bool enabled)
{
	g_shuffle = enabled;
	Save();
}

int BgmCatalog::Pick(int avoid)
{
	std::vector<int> pool;

	for (int index = 0; index < Count(); ++index)
	{
		const int id = IdAt(index);

		if (IsListed(id) && IsAllowed(id))
			pool.push_back(id);
	}

	if (pool.empty())
		return -1;

	if (pool.size() == 1)
		return pool[0];

	for (int attempt = 0; attempt < 8; ++attempt)
	{
		const int pick = pool[NextRandom() % pool.size()];

		if (pick != avoid)
			return pick;
	}

	return pool[NextRandom() % pool.size()];
}

void BgmCatalog::Load()
{
	const std::string path = IniPath();

	g_refused.clear();
	g_shuffle = GetPrivateProfileIntA("Shuffle", "Enabled", 0, path.c_str()) != 0;

	const int count = static_cast<int>(GetPrivateProfileIntA("Shuffle", "Refused", 0,
		path.c_str()));

	for (int i = 0; i < count && i < kMaxRefused; ++i)
	{
		char key[24] = {};
		sprintf_s(key, "Off%d", i);

		char value[64] = {};
		GetPrivateProfileStringA("Shuffle", key, "", value, sizeof(value), path.c_str());

		if (value[0] == 0)
			continue;

		std::string ref = value;

		for (char& c : ref)
			c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

		g_refused.insert(ref);
	}

	LOG("BgmCatalog: shuffle is %s, %d track(s) held back",
		g_shuffle ? "on" : "off", static_cast<int>(g_refused.size()));
}

void BgmCatalog::Save()
{
	const std::string path = IniPath();

	WritePrivateProfileStringA("Shuffle", nullptr, nullptr, path.c_str());
	WritePrivateProfileStringA("Shuffle", "Enabled", g_shuffle ? "1" : "0", path.c_str());

	char count[24] = {};
	sprintf_s(count, "%d", static_cast<int>(g_refused.size()));
	WritePrivateProfileStringA("Shuffle", "Refused", count, path.c_str());

	int index = 0;

	for (const std::string& ref : g_refused)
	{
		char key[24] = {};
		sprintf_s(key, "Off%d", index);
		WritePrivateProfileStringA("Shuffle", key, ref.c_str(), path.c_str());
		++index;
	}
}
