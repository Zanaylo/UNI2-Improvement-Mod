// MinHook wrapper plus the pattern, string and RTTI scanners used to locate game functions.

#pragma once

#include <Windows.h>
#include <cstdint>

namespace HookManager
{
	bool Initialize();
	void Shutdown();

	bool CreateHook(void* target, void* detour, void** original, const char* label);

	bool CreateAndEnableHook(void* target, void* detour, void** original, const char* label);
	bool CreateApiHook(const char* moduleName, const char* functionName, void* detour, void** original);
	bool CreateVTableHook(void* instance, int index, void* detour, void** original, const char* label);
	bool EnableAllHooks();
	bool SetHookEnabled(void* target, bool enabled);

	uintptr_t FindPattern(const char* pattern, const char* mask);
	uintptr_t FindPatternInRange(uintptr_t start, size_t size, const char* pattern, const char* mask);

	uintptr_t FindString(const char* text);
	uintptr_t FindReferenceTo(uintptr_t address);
	uintptr_t FindRttiVTable(const char* mangledName);

	bool GetSectionBounds(const char* sectionName, uintptr_t& outStart, size_t& outSize);
	bool WaitForModule(const char* moduleName, DWORD timeoutMs);
}
