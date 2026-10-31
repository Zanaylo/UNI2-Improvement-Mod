#include "Game/Lobby/NameCensor.h"

#include "Core/logger.h"
#include "Game/Lobby/PersonaNameHook.h"
#include "Network/Steam/SteamInterfaces.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kSeenMax = 64;

bool g_installed = false;
bool g_enabled = false;
bool g_coversMyName = false;
uint64_t g_seen[kSeenMax] = {};
volatile long g_seenCount = 0;

char g_mask[NameCensor::kMaskMax + 1] = "Player";
char g_status[160] = "the game's persona name resolver is not where this game version expects it";

uint64_t g_selfId = 0;

uint64_t SelfId()
{
	if (g_selfId == 0)
		g_selfId = SteamInterfaces::GetLocalSteamId();

	return g_selfId;
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
	if (!g_installed)
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

	sprintf_s(g_status, "on, everyone%s is shown as '%s'", g_coversMyName ? "" : " but you",
		g_mask);
}

}

bool NameCensor::Install()
{
	g_installed = PersonaNameHook::Install();

	Summarise();
	LOG("NameCensor: %s", g_status);
	return g_installed;
}

bool NameCensor::IsAvailable()
{
	return g_installed;
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

bool NameCensor::CoversMyName()
{
	return g_coversMyName;
}

void NameCensor::SetCoversMyName(bool covers)
{
	if (covers == g_coversMyName)
		return;

	g_coversMyName = covers;
	Summarise();
}

bool NameCensor::Applies(uint64_t steamId)
{
	if (!g_installed || !g_enabled)
		return false;

	if (g_coversMyName)
		return true;

	const uint64_t self = SelfId();

	return self == 0 || self != steamId;
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
