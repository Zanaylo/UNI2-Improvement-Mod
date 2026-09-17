#include "Core/ModuleInventory.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <cstring>

namespace {

constexpr DWORD kMostModules = 512;

bool IsModuleOfThisMod(HMODULE module)
{
	return module == GetModModuleHandle() || module == GetModuleHandleA(nullptr);
}

}

void ModuleInventory::LogForeignModules(const char* when)
{
	HMODULE modules[kMostModules] = {};
	DWORD needed = 0;

	if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed))
	{
		LOG("Modules %s: the list could not be read", when);
		return;
	}

	const DWORD count = (std::min)(needed / sizeof(HMODULE), kMostModules);
	int listed = 0;

	for (DWORD i = 0; i < count; ++i)
	{
		char path[MAX_PATH] = {};

		if (IsModuleOfThisMod(modules[i]) || GetModuleFileNameA(modules[i], path, MAX_PATH) == 0 ||
			IsUnderSystemDirectory(path))
		{
			continue;
		}

		LOG("Modules %s: 0x%p %s", when, static_cast<void*>(modules[i]), path);
		++listed;
	}

	LOG("Modules %s: %d loaded from outside Windows' own folders", when, listed);
}
