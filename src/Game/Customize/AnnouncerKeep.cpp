#include "Game/Customize/AnnouncerKeep.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Customize/AnnouncerList.h"
#include "Game/Customize/AnnouncerTicks.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Files/DataArchive.h"
#include "Hooks/GameHook.h"

#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char* kCustomize = "grpdat\\Customize";
constexpr const char* kAnnouncerList = "AnnounceCharaList.csv";

using SanitiseFn = void(__cdecl*)();

GameHook<SanitiseFn> g_sanitiseHook("CustomizeSanitise");
volatile long g_runs = 0;
AnnouncerTicks::Slots g_imported = {};

uint8_t* GameBytes(uintptr_t rva)
{
	return reinterpret_cast<uint8_t*>(RvaToAddress(rva));
}

std::vector<int> StockSaveIds()
{
	std::vector<uint8_t> csv;

	if (!DataArchive::Read(kCustomize, kAnnouncerList, csv))
		return {};

	return AnnouncerList::SaveIds(std::string(csv.begin(), csv.end()));
}

int ImportedCount()
{
	return static_cast<int>(std::count(g_imported.begin(), g_imported.end(), true));
}

void __cdecl SanitiseKeepingImports()
{
	uint8_t* const ticked = GameBytes(GameOffsets::kAnnouncerTicked);
	uint8_t* const excluded = GameBytes(GameOffsets::kAnnouncerExcluded);
	AnnouncerTicks::Flags before = {};
	memcpy(before.data(), ticked, before.size());

	g_sanitiseHook.Original()();

	AnnouncerTicks::Keep(g_imported, before, ticked, excluded);

	const long runs = InterlockedIncrement(&g_runs);
	LOG("AnnouncerKeep: the save sanitiser ran (%ld), imported announcer ticks kept", runs);
}

}

bool AnnouncerKeep::Install()
{
	if (g_sanitiseHook.IsLive())
		return true;

	g_imported = AnnouncerTicks::Imported(StockSaveIds());

	if (ImportedCount() == 0)
	{
		LOG("AnnouncerKeep: the game's announcer list could not be read, so no tick is kept");
		return false;
	}

	const bool live = g_sanitiseHook.InstallRva(GameOffsets::kFnCustomizeSanitise, &SanitiseKeepingImports);
	LOG("AnnouncerKeep: %s, %d save ids are free for imports", live ? "watching the save sanitiser" : "not installed",
		ImportedCount());
	return live;
}

long AnnouncerKeep::SanitiserRuns()
{
	return g_runs;
}
