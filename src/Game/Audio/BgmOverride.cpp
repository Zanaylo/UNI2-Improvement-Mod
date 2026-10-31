#include "Game/Audio/BgmOverride.h"

#include "Core/FileIndex.h"
#include "Core/FileSystem.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Audio/BgmLayers.h"
#include "Game/Audio/BgmPicker.h"
#include "Game/Audio/BgmTable.h"
#include "Game/Audio/BgmTableFile.h"
#include "Game/Files/ModPacks.h"

#include <Windows.h>

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* kServedTable = "Bgm\\bgm.txt";
constexpr const char* kServedSelect = "grpdat\\CSel\\bgmselect.txt";
constexpr const char* kUserTable = "Mods\\Bgm\\bgm.txt";
constexpr const char* kGeneratedFolder = "Generated";
constexpr const char* kGeneratedTable = "Generated\\bgm.txt";
constexpr const char* kUserName = "Your files";

struct Source
{
	std::string folder;
	bool yours;
};

SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<Source> g_sources;
std::vector<BgmLayers::Placed> g_placed;
std::string g_gameTable;
bool g_gameTableRead = false;

bool ReadText(const std::string& path, std::string& out)
{
	std::vector<uint8_t> blob;

	if (!ReadWholeFile(path, blob) || blob.empty())
		return false;

	out.assign(blob.begin(), blob.end());
	return true;
}

const std::string& GameTable()
{
	if (g_gameTableRead)
		return g_gameTable;

	std::vector<uint8_t> blob;
	BgmTableFile::ReadGameTable(blob);

	g_gameTable.assign(blob.begin(), blob.end());
	g_gameTableRead = true;

	return g_gameTable;
}

std::string PackFile(const std::string& folder, const char* gamePath)
{
	return folder + "\\" + gamePath;
}

bool WriteIfChanged(const std::string& path, const std::string& text)
{
	std::string was;

	if (ReadText(path, was) && was == text)
		return false;

	CreateDirectoryA(GetModRootPath(kGeneratedFolder).c_str(), nullptr);
	return WriteWholeFile(path, std::vector<uint8_t>(text.begin(), text.end()));
}

int LayerOf(const std::vector<Source>& sources, const std::string& folder)
{
	for (size_t i = 0; i < sources.size(); ++i)
	{
		if (!sources[i].yours && sources[i].folder == folder)
			return static_cast<int>(i);
	}

	return -1;
}

void Remember(const std::vector<Source>& sources, const std::vector<BgmLayers::Placed>& placed)
{
	AcquireSRWLockExclusive(&g_lock);
	g_sources = sources;
	g_placed = placed;
	ReleaseSRWLockExclusive(&g_lock);
}

void LogPlaced(const std::vector<BgmLayers::Layer>& layers, const std::vector<BgmLayers::Placed>& placed,
	int lastPackSlot)
{
	for (const BgmLayers::Placed& one : placed)
	{
		if (one.to == BgmLayers::kNoSlot)
		{
			LOG("BgmOverride: music slot %d of %s is left out, every slot up to %d is in use", one.from,
				layers[one.layer].name.c_str(), lastPackSlot);
			continue;
		}

		if (one.to != one.from)
		{
			LOG("BgmOverride: music slot %d of %s is used higher in the list, so its track plays from slot %d",
				one.from, layers[one.layer].name.c_str(), one.to);
		}
	}
}

}

void BgmOverride::Layer(FileIndex& into)
{
	std::vector<Source> sources;
	std::vector<BgmLayers::Layer> layers;
	bool packTable = false;

	std::string userTable;

	if (ReadText(GetModRootPath(kUserTable), userTable))
	{
		sources.push_back({ std::string(), true });
		layers.push_back({ kUserName, userTable });
	}

	for (const ModPacks::Pack& pack : ModPacks::EnabledTopDown())
	{
		std::string table;
		packTable = ReadText(PackFile(pack.path, kServedTable), table) || packTable;

		sources.push_back({ pack.path, false });
		layers.push_back({ pack.name, table });
	}

	if (!packTable)
	{
		Remember(sources, std::vector<BgmLayers::Placed>());
		return;
	}

	AcquireSRWLockExclusive(&g_lock);
	const std::string base = GameTable();
	ReleaseSRWLockExclusive(&g_lock);

	if (base.empty())
	{
		Remember(sources, std::vector<BgmLayers::Placed>());
		LOG("BgmOverride: the game's music table could not be found, so the highest bgm.txt is used as it is");
		return;
	}

	const int lastPackSlot = BgmTable::LastPackSlot();
	const BgmLayers::Composed composed = BgmLayers::Compose(base, layers, lastPackSlot);
	const std::string path = GetModRootPath(kGeneratedTable);

	Remember(sources, composed.placed);

	if (WriteIfChanged(path, composed.table))
	{
		LOG("BgmOverride: the music table is composed from %d layer(s), %d pack track(s)",
			static_cast<int>(layers.size()), static_cast<int>(composed.placed.size()));
		LogPlaced(layers, composed.placed, lastPackSlot);
	}

	into.Add(FileIndex::Key(std::string(kServedTable)), path);
}

bool BgmOverride::IsTable(const std::string& key)
{
	if (key == FileIndex::Key(std::string(kServedTable)))
		return true;

	return BgmPicker::Active() && key == FileIndex::Key(std::string(kServedSelect));
}

int BgmOverride::AddPackRecords(BgmSelectFile::Contents& contents, int limit)
{
	AcquireSRWLockShared(&g_lock);
	const std::vector<Source> sources = g_sources;
	const std::vector<BgmLayers::Placed> placed = g_placed;
	ReleaseSRWLockShared(&g_lock);

	std::unordered_set<int> listed;

	for (const BgmSelectFile::Record& record : contents.records)
		listed.insert(record.number);

	int added = 0;

	for (size_t layer = 0; layer < sources.size(); ++layer)
	{
		std::vector<uint8_t> blob;

		if (sources[layer].yours || !ReadWholeFile(PackFile(sources[layer].folder, kServedSelect), blob))
			continue;

		BgmSelectFile::Contents theirs;
		BgmSelectFile::Split(blob, theirs);

		for (const BgmSelectFile::Record& record : theirs.records)
		{
			if (static_cast<int>(contents.records.size()) >= limit)
				return added;

			const int number = BgmLayers::PlacedNumber(placed, static_cast<int>(layer), record.number);

			if (number == BgmLayers::kNoSlot || !listed.insert(number).second)
				continue;

			contents.records.push_back({ number,
				number == record.number ? record.body : BgmLayers::Renumbered(record.body, number) });
			++added;
		}
	}

	return added;
}

bool BgmOverride::Report(const std::string& packPath, PackMusic& out)
{
	out = PackMusic();

	AcquireSRWLockShared(&g_lock);

	const int layer = LayerOf(g_sources, packPath);

	for (const BgmLayers::Placed& one : g_placed)
	{
		if (one.layer != layer)
			continue;

		out.tracks += one.to == BgmLayers::kNoSlot ? 0 : 1;
		out.moved += one.to != BgmLayers::kNoSlot && one.to != one.from ? 1 : 0;
		out.dropped += one.to == BgmLayers::kNoSlot ? 1 : 0;
	}

	ReleaseSRWLockShared(&g_lock);

	return out.tracks + out.dropped > 0;
}
