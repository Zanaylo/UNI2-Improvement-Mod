#include "Game/StageObjects.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>

namespace {

using LoadPat_t = void*(__fastcall*)(void*, void*, const char*);

LoadPat_t oLoadPat = nullptr;

char g_status[224] = "no stage with an object layer loaded yet";
volatile long g_loads = 0;
volatile long g_failures = 0;

void* __fastcall HookedLoadPat(void* self, void* unused, const char* path)
{
	void* const result = oLoadPat(self, unused, path);

	InterlockedIncrement(&g_loads);

	if (result == nullptr)
		InterlockedIncrement(&g_failures);

	sprintf_s(g_status, "%s %s", path == nullptr ? "(no path)" : path,
		result == nullptr ? "did not load, so the stage has no objects" : "loaded");

	LOG("StageObjects: %s", g_status);

	return result;
}

}

bool StageObjects::Initialize()
{
	const uintptr_t address = RvaToAddress(GameOffsets::kFnLoadStageObjectPat);

	if (!IsAddressInGameModule(address))
	{
		strncpy_s(g_status, "the stage object loader is not where this game version expects it", _TRUNCATE);
		LOG("StageObjects: %s", g_status);
		return false;
	}

	if (!HookManager::CreateAndEnableHook(reinterpret_cast<void*>(address), &HookedLoadPat,
		reinterpret_cast<void**>(&oLoadPat), "StageObjectPat"))
	{
		strncpy_s(g_status, "the stage object loader could not be hooked", _TRUNCATE);
		return false;
	}

	return true;
}

int StageObjects::Loads()
{
	return static_cast<int>(InterlockedCompareExchange(&g_loads, 0, 0));
}

int StageObjects::Failures()
{
	return static_cast<int>(InterlockedCompareExchange(&g_failures, 0, 0));
}

const char* StageObjects::StatusText()
{
	return g_status;
}
