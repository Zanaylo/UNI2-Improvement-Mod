#include "Game/BgmVolume.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmTable.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace {

constexpr const char* kSection = "Volume";
constexpr int kEngineFull = 10000;

typedef void(__fastcall* SetVolumeFn)(int base, int track);
typedef int(__fastcall* StreamVolumeFn)(void* stream, void* unused, int scalar);

std::map<std::string, int> g_picks;
int g_original[BgmTable::kSlotCount] = {};
bool g_written[BgmTable::kSlotCount] = {};

SetVolumeFn oSetVolume = nullptr;
StreamVolumeFn oStreamVolume = nullptr;

bool g_inRoutine = false;
int g_reportedScalar = -1;

int g_revision = 0;
int g_currentSlot = -1;
int g_currentId = -1;

int g_cachedSlot = -1;
int g_cachedId = -1;
int g_cachedRevision = -1;
int g_cachedValue = 0;
bool g_cached = false;

std::string IniPath()
{
	return GetModRootPath("bgm.ini");
}

int Clamp(int percent)
{
	if (percent < 0)
		return 0;

	if (percent > BgmVolume::kFullPercent)
		return BgmVolume::kFullPercent;

	return percent;
}

const int* Find(int id)
{
	const std::map<std::string, int>::const_iterator found = g_picks.find(BgmLibrary::RefKey(id));

	if (found == g_picks.end())
		return nullptr;

	return &found->second;
}

bool Wanted(int slot, int id, int& out)
{
	if (slot < 0 || slot >= BgmTable::kSlotCount)
		return false;

	if (BgmVolume::IsCustom(id))
	{
		out = BgmVolume::EngineValue(BgmVolume::Get(id));
		return true;
	}

	if (!g_written[slot])
		return false;

	out = g_original[slot];
	return true;
}

bool CurrentWanted(int& out, int& slotOut, int& idOut)
{
	uint32_t loaded = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kBgmCurrentId)),
		loaded))
	{
		return false;
	}

	const int slot = static_cast<int>(loaded);
	const int id = slot == g_currentSlot ? g_currentId : slot;

	slotOut = slot;
	idOut = id;

	if (g_cachedSlot != slot || g_cachedId != id || g_cachedRevision != g_revision)
	{
		g_cachedSlot = slot;
		g_cachedId = id;
		g_cachedRevision = g_revision;
		g_cached = Wanted(slot, id, g_cachedValue);
	}

	out = g_cachedValue;
	return g_cached;
}

bool CurrentWanted(int& out)
{
	int slot = -1;
	int id = -1;

	return CurrentWanted(out, slot, id);
}

void WriteScalar(int engineVolume)
{
	TryWriteDword(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kBgmTrackVolume)),
		static_cast<uint32_t>(engineVolume));
}

int Scaled(int level, int wanted)
{
	return static_cast<int>(static_cast<int64_t>(level) * wanted / kEngineFull);
}

void CallRoutine(int base, int track)
{
	g_inRoutine = true;
	oSetVolume(base, track);
	g_inRoutine = false;
}

void __fastcall HookedSetVolume(int base, int track)
{
	int wanted = 0;
	int slot = -1;
	int id = -1;

	const bool forced = CurrentWanted(wanted, slot, id);

	LOG("BgmVolume: base %d track %d, slot %d id %d, ours %d/%d, %s %d", base, track, slot, id,
		g_currentSlot, g_currentId, forced ? "forcing" : "passing through", forced ? wanted : track);

	if (!forced)
	{
		CallRoutine(base, track);
		return;
	}

	WriteScalar(wanted);
	CallRoutine(base, wanted);
}

bool IsBgmStream(const void* stream)
{
	uint32_t player = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kBgmPlayer)), player))
		return false;

	return player != 0 && reinterpret_cast<uintptr_t>(stream) == player;
}

int __fastcall HookedStreamVolume(void* stream, void* unused, int scalar)
{
	int wanted = 0;
	int slot = -1;
	int id = -1;

	if (g_inRoutine || scalar <= 0 || !IsBgmStream(stream) || !CurrentWanted(wanted, slot, id))
		return oStreamVolume(stream, unused, scalar);

	const int scaled = Scaled(scalar, wanted);

	if (scalar != g_reportedScalar)
	{
		g_reportedScalar = scalar;

		LOG("BgmVolume: a scene set the BGM straight to %d, slot %d id %d, holding it to %d", scalar,
			slot, id, scaled);
	}

	return oStreamVolume(stream, unused, scaled);
}

void InstallStreamHook()
{
	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnStreamSetVolume));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
		return;

	if (HookManager::CreateAndEnableHook(target, &HookedStreamVolume,
		reinterpret_cast<void**>(&oStreamVolume), "StreamSetVolume"))
	{
		return;
	}

	oStreamVolume = nullptr;
	LOG("BgmVolume: the stream level could not be hooked, the scenes that set it themselves will "
		"play at full volume");
}

}

void BgmVolume::Load()
{
	g_picks.clear();

	for (int slot = 0; slot < BgmTable::kSlotCount; ++slot)
	{
		g_original[slot] = kEngineFull;
		g_written[slot] = false;
	}

	const std::string path = IniPath();

	char section[8192] = {};
	const DWORD read = GetPrivateProfileSectionA(kSection, section, sizeof(section), path.c_str());

	if (read == 0)
		return;

	for (const char* entry = section; *entry != '\0'; entry += strlen(entry) + 1)
	{
		const char* const separator = strchr(entry, '=');

		if (separator == nullptr || separator == entry)
			continue;

		g_picks[std::string(entry, separator)] = Clamp(atoi(separator + 1));
	}

	LOG("BgmVolume: %d track(s) with a volume of their own", static_cast<int>(g_picks.size()));
}

void BgmVolume::Save()
{
	const std::string path = IniPath();

	WritePrivateProfileStringA(kSection, nullptr, nullptr, path.c_str());

	for (const std::pair<const std::string, int>& pick : g_picks)
	{
		char value[16] = {};
		sprintf_s(value, "%d", pick.second);

		WritePrivateProfileStringA(kSection, pick.first.c_str(), value, path.c_str());
	}
}

int BgmVolume::Get(int id)
{
	const int* const pick = Find(id);

	return pick != nullptr ? *pick : kFullPercent;
}

void BgmVolume::Set(int id, int percent)
{
	const std::string key = BgmLibrary::RefKey(id);

	if (key.empty())
		return;

	const int wanted = Clamp(percent);

	if (wanted == kFullPercent)
		g_picks.erase(key);
	else
		g_picks[key] = wanted;

	++g_revision;
}

bool BgmVolume::IsCustom(int id)
{
	return Find(id) != nullptr;
}

int BgmVolume::CustomCount()
{
	return static_cast<int>(g_picks.size());
}

void BgmVolume::ResetAll()
{
	g_picks.clear();
	++g_revision;
	Save();
}

int BgmVolume::EngineValue(int percent)
{
	return Clamp(percent) * (kEngineFull / kFullPercent);
}

void BgmVolume::ApplyToSlot(int slot, int id)
{
	if (slot < 0 || slot >= BgmTable::kSlotCount)
		return;

	const bool custom = IsCustom(id);

	if (!custom && !g_written[slot])
		return;

	if (!custom)
	{
		BgmTable::SetVolume(slot, g_original[slot]);
		g_written[slot] = false;
		return;
	}

	if (!g_written[slot])
	{
		int current = kEngineFull;

		if (BgmTable::GetVolume(slot, current))
			g_original[slot] = current;

		g_written[slot] = true;
	}

	BgmTable::SetVolume(slot, EngineValue(Get(id)));
}

void BgmVolume::SetCurrent(int slot, int id)
{
	g_currentSlot = slot;
	g_currentId = id;
}

void BgmVolume::ApplyNow()
{
	int wanted = 0;

	if (oSetVolume == nullptr || !CurrentWanted(wanted))
		return;

	uint32_t base = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kBgmBaseVolume)),
		base))
	{
		return;
	}

	WriteScalar(wanted);
	CallRoutine(static_cast<int>(base), wanted);
}

bool BgmVolume::Install()
{
	InstallStreamHook();

	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnBgmSetVolume));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		LOG("BgmVolume: the volume routine is outside the game module, picks will only apply on "
			"a track change");
		return false;
	}

	if (HookManager::CreateAndEnableHook(target, &HookedSetVolume,
		reinterpret_cast<void**>(&oSetVolume), "BgmSetVolume"))
	{
		return true;
	}

	oSetVolume = nullptr;
	LOG("BgmVolume: the volume routine could not be hooked, picks will only apply on a track "
		"change");
	return false;
}
