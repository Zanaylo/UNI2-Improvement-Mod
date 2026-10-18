#include "Game/Audio/BgmPicker.h"

#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/FileIndex.h"
#include "Core/Formats/TextEncoding.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Audio/BgmLibrary.h"
#include "Game/Audio/BgmNames.h"
#include "Game/Audio/BgmSelectFile.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/CompareTrampoline.h"
#include "Hooks/GameHook.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using Selectable_t = bool(__fastcall*)(int);
using SelectBuild_t = void(__cdecl*)();

constexpr const char* kSection = "Music";
constexpr const char* kKey = "ListEveryTrack";
constexpr const char* kServedKey = "grpdat\\CSel\\bgmselect.txt";
constexpr const char* kGeneratedFolder = "Generated";
constexpr const char* kGeneratedFile = "Generated\\bgmselect.txt";
constexpr int kWideSections = 1024;
constexpr int kNameBytes = 192;

GameHook<Selectable_t> g_selectableHook("BgmSelectable");
GameHook<SelectBuild_t> g_buildHook("BgmSelectBuild");

SRWLOCK g_writeLock = SRWLOCK_INIT;
bool g_active = false;
bool g_widened = false;
bool g_served = false;
char g_status[192] = "not started";

bool Wanted()
{
	char stored[16] = {};

	Ini::GetString(kSection, kKey, "1", stored, sizeof(stored), Settings::GetIniPath().c_str());

	return stored[0] != '0';
}

std::string GeneratedPath()
{
	return GetModRootPath(kGeneratedFile);
}

int SectionLimit()
{
	return g_widened ? kWideSections : GameOffsets::kBgmSelectStockSections;
}

std::string GameName(int id)
{
	char name[kNameBytes] = {};
	BgmNames::Describe(id, name, sizeof(name));

	std::string encoded;

	if (!TextEncoding::Utf8ToShiftJis(name, encoded))
		return name;

	return encoded;
}

int PickerNumber(int id)
{
	const int slot = BgmLibrary::SlotOf(id);

	return slot >= 0 ? slot : id;
}

int AddLibrary(BgmSelectFile::Contents& contents)
{
	std::unordered_set<int> listed;

	for (const BgmSelectFile::Record& record : contents.records)
		listed.insert(record.number);

	const int count = BgmLibrary::Count();
	int added = 0;

	for (int i = 0; i < count && static_cast<int>(contents.records.size()) < SectionLimit(); ++i)
	{
		const int id = BgmLibrary::IdAt(i);
		const int number = PickerNumber(id);

		if (!listed.insert(number).second)
			continue;

		contents.records.push_back({ number, BgmSelectFile::Body(number, GameName(id)) });
		++added;
	}

	return added;
}

bool Store(const std::string& text)
{
	CreateDirectoryA(GetModRootPath(kGeneratedFolder).c_str(), nullptr);

	AcquireSRWLockExclusive(&g_writeLock);
	const bool written = WriteWholeFile(GeneratedPath(), std::vector<uint8_t>(text.begin(), text.end()));
	ReleaseSRWLockExclusive(&g_writeLock);

	return written;
}

bool Write()
{
	BgmSelectFile::Contents contents;
	BgmSelectFile::Split(BgmSelectFile::ReadBase(), contents);

	if (contents.records.empty())
	{
		strncpy_s(g_status, "the game's own music list could not be read", _TRUNCATE);
		LOG("BgmPicker: %s", g_status);
		return false;
	}

	const int own = static_cast<int>(contents.records.size());
	const int added = AddLibrary(contents);

	if (!Store(BgmSelectFile::Compose(contents)))
	{
		strncpy_s(g_status, "the music list could not be written to the Generated folder", _TRUNCATE);
		LOG("BgmPicker: %s", g_status);
		return false;
	}

	sprintf_s(g_status, "%d track(s) in the game's music list, %d added by the mod%s", own + added,
		added, g_widened ? "" : " (the game's 100 entry limit is still in place)");
	LOG("BgmPicker: %s", g_status);
	return true;
}

bool AlreadyBuilt()
{
	uint8_t built = 0;
	TryReadMemory(&built, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kBgmSelectBuilt)),
		sizeof(built));

	return built != 0;
}

void __cdecl HookedBuild()
{
	if (g_served && !AlreadyBuilt())
		Write();

	g_buildHook.Original()();
}

bool __fastcall HookedSelectable(int id)
{
	if (BgmLibrary::Get(id) != nullptr)
		return true;

	return g_selectableHook.Original()(id);
}

bool Widen()
{
	const uintptr_t address = CodeSignatures::Address(GameOffsets::kSiteBgmSelectSections);

	if (!IsAddressInGameModule(address))
		return false;

	CompareTrampoline::Site site = {};

	if (!CompareTrampoline::Decode(reinterpret_cast<uint8_t*>(address), site) ||
		site.counted != GameOffsets::kBgmSelectStockSections)
	{
		return false;
	}

	uint8_t* const trampoline = static_cast<uint8_t*>(VirtualAlloc(nullptr, CompareTrampoline::kBytes,
		MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

	if (trampoline == nullptr)
		return false;

	return CompareTrampoline::Emit(site, trampoline, kWideSections);
}

bool Fail(const char* why)
{
	strncpy_s(g_status, why, _TRUNCATE);
	LOG("BgmPicker: %s", g_status);
	return false;
}

}

bool BgmPicker::Initialize()
{
	if (!Wanted())
		return Fail("off, the game's music list only holds its own tracks");

	if (!g_buildHook.InstallRva(GameOffsets::kFnBgmSelectBuild, &HookedBuild))
		return Fail("the game's music list is not where this game version expects it");

	if (!g_selectableHook.InstallRva(GameOffsets::kFnBgmSelectable, &HookedSelectable))
		return Fail("the game's music list check could not be hooked");

	g_widened = Widen();
	g_active = true;

	sprintf_s(g_status, "on, up to %d entries", SectionLimit());
	LOG("BgmPicker: %s%s", g_status, g_widened ? "" : " - the 100 entry limit could not be lifted");
	return true;
}

void BgmPicker::Layer(FileIndex& into)
{
	if (!g_active)
		return;

	g_served = Write();

	if (!g_served)
		return;

	into.Add(FileIndex::Key(std::string(kServedKey)), GeneratedPath());
}

bool BgmPicker::Active()
{
	return g_served;
}

bool BgmPicker::Widened()
{
	return g_widened;
}

const char* BgmPicker::StatusText()
{
	return g_status;
}
