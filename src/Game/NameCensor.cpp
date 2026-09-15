#include "Game/NameCensor.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"
#include "Network/SteamInterfaces.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

typedef void(__fastcall* ResolveNameFn)(const uint64_t*, char*);

constexpr int kSeenMax = 64;

ResolveNameFn oResolveName = nullptr;

bool g_enabled = false;
bool g_coversOwn = false;
uint64_t g_seen[kSeenMax] = {};
volatile long g_seenCount = 0;

char g_mask[NameCensor::kMaskMax + 1] = "Player";
char g_status[160] = "the game's persona name resolver is not where this game version expects it";

uint64_t g_own = 0;

uint64_t OwnId()
{
	if (g_own == 0)
		g_own = SteamInterfaces::GetOwnSteamId();

	return g_own;
}

void Remember(uint64_t steamId)
{
	const long count = g_seenCount;

	for (long i = 0; i < count; ++i)
	{
		if (g_seen[i] == steamId)
			return;
	}

	if (count >= kSeenMax)
		return;

	g_seen[count] = steamId;
	InterlockedIncrement(&g_seenCount);
}

void Summarise()
{
	if (oResolveName == nullptr)
	{
		strncpy_s(g_status, "the game's player name code is not where this game version expects it",
			_TRUNCATE);
		return;
	}

	if (!g_enabled)
	{
		strncpy_s(g_status, "off, names are shown as they are", _TRUNCATE);
		return;
	}

	sprintf_s(g_status, "on, everyone%s is shown as '%s'", g_coversOwn ? "" : " but you",
		g_mask);
}

void __fastcall HookedResolveName(const uint64_t* id, char* out)
{
	oResolveName(id, out);

	if (id == nullptr)
		return;

	NameCensor::Apply(*id, out);
}

}

bool NameCensor::Install()
{
	if (oResolveName != nullptr)
		return true;

	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnResolvePersonaName));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		Summarise();
		LOG("NameCensor: %s", g_status);
		return false;
	}

	if (!HookManager::CreateAndEnableHook(target, &HookedResolveName,
		reinterpret_cast<void**>(&oResolveName), "ResolvePersonaName"))
	{
		oResolveName = nullptr;
		Summarise();
		LOG("NameCensor: %s", g_status);
		return false;
	}

	Summarise();
	LOG("NameCensor: %s", g_status);
	return true;
}

bool NameCensor::IsAvailable()
{
	return oResolveName != nullptr;
}

bool NameCensor::IsEnabled()
{
	return g_enabled;
}

void NameCensor::SetEnabled(bool enabled)
{
	if (enabled == g_enabled)
		return;

	g_enabled = enabled;
	Summarise();
}

const char* NameCensor::Mask()
{
	return g_mask;
}

void NameCensor::SetMask(const char* text)
{
	if (text == nullptr || text[0] == 0)
		return;

	strncpy_s(g_mask, text, _TRUNCATE);
	Summarise();
}

bool NameCensor::CoversOwnName()
{
	return g_coversOwn;
}

void NameCensor::SetCoversOwnName(bool covers)
{
	if (covers == g_coversOwn)
		return;

	g_coversOwn = covers;
	Summarise();
}

bool NameCensor::Applies(uint64_t steamId)
{
	if (!g_enabled)
		return false;

	if (g_coversOwn)
		return true;

	const uint64_t own = OwnId();

	return own == 0 || own != steamId;
}

void NameCensor::Apply(uint64_t steamId, char* text)
{
	if (text == nullptr || !Applies(steamId))
		return;

	if (text[0] == 0)
		return;

	const size_t written = strlen(g_mask);

	memcpy(text, g_mask, written);
	text[written] = 0;

	Remember(steamId);
}

int NameCensor::Count()
{
	return static_cast<int>(g_seenCount);
}

const char* NameCensor::StatusText()
{
	return g_status;
}
