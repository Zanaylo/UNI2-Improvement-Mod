#include "Game/ScreenShake.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr size_t kQuakeListSize = 0x44;
constexpr int kSilent = -100;

typedef int(__fastcall* CameraQuakeFn)(void* self, void* unused, int time, int type, int value);

CameraQuakeFn oCameraQuake = nullptr;
int g_percent = ScreenShake::kFullPercent;
char g_status[192] = "the camera's quake list is not where this build expects it";

int Scaled(int value)
{
	const int wanted = g_percent * (value + ScreenShake::kFullPercent) / ScreenShake::kFullPercent
		- ScreenShake::kFullPercent;

	return wanted < kSilent ? kSilent : wanted;
}

int __fastcall HookedCameraQuake(void* self, void* unused, int time, int type, int value)
{
	if (g_percent >= ScreenShake::kFullPercent)
		return oCameraQuake(self, unused, time, type, value);

	if (g_percent <= 0)
		return oCameraQuake(self, unused, 0, type, value);

	return oCameraQuake(self, unused, time, type, Scaled(value));
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
	if (oCameraQuake == nullptr)
	{
		strncpy_s(g_status, "the camera's quake list is not where this build expects it",
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
		strncpy_s(g_status, "screen shake is held off", _TRUNCATE);
		return;
	}

	sprintf_s(g_status, "screen shake at %d%% of the game's own", g_percent);
}

}

bool ScreenShake::Install()
{
	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnCameraQuake));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		Summarise();
		LOG("ScreenShake: %s", g_status);
		return false;
	}

	if (!HookManager::CreateAndEnableHook(target, &HookedCameraQuake,
		reinterpret_cast<void**>(&oCameraQuake), "CameraQuake"))
	{
		oCameraQuake = nullptr;
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
	return oCameraQuake != nullptr;
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
