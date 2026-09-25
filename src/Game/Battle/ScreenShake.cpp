#include "Game/Battle/ScreenShake.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/GameHook.h"
#include "Game/Engine/CodeSignatures.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr size_t kQuakeListSize = 0x44;
constexpr int kSilent = -100;

typedef int(__fastcall* CameraQuakeFn)(void* self, void* unused, int time, int type, int value);

GameHook<CameraQuakeFn> g_cameraQuakeHook("CameraQuake");
int g_percent = ScreenShake::kFullPercent;
char g_status[192] = "the camera's quake list is not where this game version expects it";

int Scaled(int value)
{
	const int wanted = g_percent * (value + ScreenShake::kFullPercent) / ScreenShake::kFullPercent
		- ScreenShake::kFullPercent;

	return wanted < kSilent ? kSilent : wanted;
}

int __fastcall HookedCameraQuake(void* self, void* unused, int time, int type, int value)
{
	if (g_percent >= ScreenShake::kFullPercent)
		return g_cameraQuakeHook.Original()(self, unused, time, type, value);

	if (g_percent <= 0)
		return g_cameraQuakeHook.Original()(self, unused, 0, type, value);

	return g_cameraQuakeHook.Original()(self, unused, time, type, Scaled(value));
}

void ClearPendingShakes()
{
	void* const list = reinterpret_cast<void*>(
		RvaToAddress(GameOffsets::kCameraObject + GameOffsets::kCameraQuakeList));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(list)))
		return;

	uint8_t empty[kQuakeListSize] = {};
	TryWriteMemory(list, empty, sizeof(empty));
}

int Clamp(int percent)
{
	if (percent < 0)
		return 0;

	if (percent > ScreenShake::kFullPercent)
		return ScreenShake::kFullPercent;

	return percent;
}

void Summarise()
{
	if (!g_cameraQuakeHook.IsLive())
	{
		strncpy_s(g_status, "the screen shake data is not where this game version expects it",
			_TRUNCATE);
		return;
	}

	if (g_percent >= ScreenShake::kFullPercent)
	{
		strncpy_s(g_status, "the game's own screen shake", _TRUNCATE);
		return;
	}

	if (g_percent <= 0)
	{
		strncpy_s(g_status, "screen shake is off", _TRUNCATE);
		return;
	}

	sprintf_s(g_status, "screen shake at %d%% of the game's own", g_percent);
}

}

bool ScreenShake::Install()
{
	void* const target = reinterpret_cast<void*>(CodeSignatures::Address(GameOffsets::kFnCameraQuake));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		Summarise();
		LOG("ScreenShake: %s", g_status);
		return false;
	}

	if (!g_cameraQuakeHook.Install(target, &HookedCameraQuake))
	{
		Summarise();
		LOG("ScreenShake: %s", g_status);
		return false;
	}

	Summarise();
	LOG("ScreenShake: %s", g_status);
	return true;
}

bool ScreenShake::IsAvailable()
{
	return g_cameraQuakeHook.IsLive();
}

int ScreenShake::GetIntensity()
{
	return g_percent;
}

void ScreenShake::SetIntensity(int percent)
{
	const int wanted = Clamp(percent);

	if (wanted == g_percent)
		return;

	g_percent = wanted;

	if (wanted <= 0)
		ClearPendingShakes();

	Summarise();
}

const char* ScreenShake::StatusText()
{
	return g_status;
}
