#include "Hooks/HookManager.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <MinHook.h>
#include <cstring>

namespace {

bool g_initialized = false;

bool MatchesPattern(const uint8_t* data, const char* pattern, const char* mask)
{
	for (; *mask; ++mask, ++data, ++pattern)
	{
		if (*mask == 'x' && *reinterpret_cast<const uint8_t*>(pattern) != *data)
			return false;
	}

	return true;
}

uintptr_t ScanRange(uintptr_t start, size_t size, const char* pattern, const char* mask)
{
	const size_t patternLength = strlen(mask);
	if (patternLength == 0 || size < patternLength)
		return 0;

	const uint8_t* data = reinterpret_cast<const uint8_t*>(start);
	const size_t limit = size - patternLength;

	for (size_t i = 0; i <= limit; ++i)
	{
		if (MatchesPattern(data + i, pattern, mask))
			return start + i;
	}

	return 0;
}

IMAGE_NT_HEADERS* GetNtHeaders()
{
	const uintptr_t base = GetGameBaseAddress();
	if (base == 0)
		return nullptr;

	IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return nullptr;

	IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return nullptr;

	return nt;
}

}

bool HookManager::GetSectionBounds(const char* sectionName, uintptr_t& outStart, size_t& outSize)
{
	IMAGE_NT_HEADERS* nt = GetNtHeaders();
	if (nt == nullptr)
		return false;

	const uintptr_t base = GetGameBaseAddress();
	IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);

	for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
	{
		char name[9] = {};
		memcpy(name, section->Name, 8);

		if (_stricmp(name, sectionName) == 0)
		{
			outStart = base + section->VirtualAddress;
			outSize = section->Misc.VirtualSize;
			return true;
		}
	}

	return false;
}

bool HookManager::Initialize()
{
	if (g_initialized)
		return true;

	const MH_STATUS status = MH_Initialize();
	if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
	{
		LOG("MH_Initialize failed: %d", status);
		return false;
	}

	g_initialized = true;
	LOG("HookManager initialized. Game base = 0x%p, size = 0x%zx", (void*)GetGameBaseAddress(), GetGameModuleSize());
	return true;
}

void HookManager::Shutdown()
{
	if (!g_initialized)
		return;

	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
	g_initialized = false;
}

bool HookManager::CreateHook(void* target, void* detour, void** original, const char* label)
{
	if (target == nullptr)
	{
		LOG("CreateHook '%s' failed: null target", label);
		return false;
	}

	const MH_STATUS status = MH_CreateHook(target, detour, original);
	if (status != MH_OK)
	{
		LOG("MH_CreateHook '%s' failed: %d", label, status);
		return false;
	}

	LOG("Hook created: %s at 0x%p", label, target);
	return true;
}

bool HookManager::CreateAndEnableHook(void* target, void* detour, void** original, const char* label)
{
	if (!CreateHook(target, detour, original, label))
		return false;

	const MH_STATUS status = MH_EnableHook(target);
	if (status != MH_OK)
	{
		LOG("MH_EnableHook '%s' failed: %d", label, status);
		return false;
	}

	return true;
}

bool HookManager::CreateApiHook(const char* moduleName, const char* functionName, void* detour, void** original)
{
	HMODULE hModule = GetModuleHandleA(moduleName);
	if (hModule == nullptr)
	{
		LOG("CreateApiHook failed: module %s not loaded", moduleName);
		return false;
	}

	void* target = reinterpret_cast<void*>(GetProcAddress(hModule, functionName));
	if (target == nullptr)
	{
		LOG("CreateApiHook failed: %s not found in %s", functionName, moduleName);
		return false;
	}

	return CreateHook(target, detour, original, functionName);
}

bool HookManager::CreateVTableHook(void* instance, int index, void* detour, void** original, const char* label)
{
	if (instance == nullptr)
	{
		LOG("CreateVTableHook '%s' failed: null instance", label);
		return false;
	}

	void** vtable = *reinterpret_cast<void***>(instance);
	if (!IsReadableMemory(vtable, sizeof(void*) * (index + 1)))
	{
		LOG("CreateVTableHook '%s' failed: vtable not readable", label);
		return false;
	}

	return CreateHook(vtable[index], detour, original, label);
}

bool HookManager::SetHookEnabled(void* target, bool enabled)
{
	const MH_STATUS status = enabled ? MH_EnableHook(target) : MH_DisableHook(target);
	if (status != MH_OK)
	{
		LOG("MH_%sHook failed on 0x%p: %d", enabled ? "Enable" : "Disable", target, status);
		return false;
	}

	return true;
}

bool HookManager::EnableAllHooks()
{
	const MH_STATUS status = MH_EnableHook(MH_ALL_HOOKS);
	if (status != MH_OK)
	{
		LOG("MH_EnableHook(MH_ALL_HOOKS) failed: %d", status);
		return false;
	}

	return true;
}

uintptr_t HookManager::FindPatternInRange(uintptr_t start, size_t size, const char* pattern, const char* mask)
{
	return ScanRange(start, size, pattern, mask);
}

uintptr_t HookManager::FindPattern(const char* pattern, const char* mask)
{
	uintptr_t start = 0;
	size_t size = 0;

	if (GetSectionBounds(".text", start, size))
	{
		const uintptr_t result = ScanRange(start, size, pattern, mask);
		if (result != 0)
			return result;
	}

	return 0;
}

uintptr_t HookManager::FindString(const char* text)
{
	const size_t length = strlen(text) + 1;

	static const char* const sections[] = { ".rdata", ".data" };
	for (const char* sectionName : sections)
	{
		uintptr_t start = 0;
		size_t size = 0;
		if (!GetSectionBounds(sectionName, start, size))
			continue;

		if (size < length)
			continue;

		const char* data = reinterpret_cast<const char*>(start);
		for (size_t i = 0; i <= size - length; ++i)
		{
			if (memcmp(data + i, text, length) == 0)
				return start + i;
		}
	}

	return 0;
}

uintptr_t HookManager::FindReferenceTo(uintptr_t address)
{
	if (address == 0)
		return 0;

	uintptr_t start = 0;
	size_t size = 0;
	if (!GetSectionBounds(".text", start, size))
		return 0;

	const uint32_t needle = static_cast<uint32_t>(address);
	const uint8_t* data = reinterpret_cast<const uint8_t*>(start);

	for (size_t i = 0; i + sizeof(uint32_t) <= size; ++i)
	{
		if (*reinterpret_cast<const uint32_t*>(data + i) == needle)
			return start + i;
	}

	return 0;
}

uintptr_t HookManager::FindRttiVTable(const char* mangledName)
{
	const uintptr_t nameAddress = FindString(mangledName);
	if (nameAddress == 0)
	{
		LOG("FindRttiVTable: type name '%s' not found", mangledName);
		return 0;
	}

	const uintptr_t typeDescriptor = nameAddress - (sizeof(void*) * 2);

	uintptr_t rdataStart = 0;
	size_t rdataSize = 0;
	if (!GetSectionBounds(".rdata", rdataStart, rdataSize))
		return 0;

	const uint32_t typeNeedle = static_cast<uint32_t>(typeDescriptor);
	const uint8_t* rdata = reinterpret_cast<const uint8_t*>(rdataStart);

	for (size_t i = 0; i + sizeof(uint32_t) <= rdataSize; i += sizeof(uint32_t))
	{
		if (*reinterpret_cast<const uint32_t*>(rdata + i) != typeNeedle)
			continue;

		const uintptr_t locator = rdataStart + i - (sizeof(uint32_t) * 3);
		if (!IsAddressInGameModule(locator))
			continue;

		const uint32_t locatorNeedle = static_cast<uint32_t>(locator);
		for (size_t j = 0; j + sizeof(uint32_t) <= rdataSize; j += sizeof(uint32_t))
		{
			if (*reinterpret_cast<const uint32_t*>(rdata + j) == locatorNeedle)
			{
				const uintptr_t vtable = rdataStart + j + sizeof(uint32_t);
				LOG("FindRttiVTable: '%s' -> 0x%p (rva 0x%x)", mangledName, (void*)vtable, (unsigned)(vtable - GetGameBaseAddress()));
				return vtable;
			}
		}
	}

	LOG("FindRttiVTable: no vtable found for '%s'", mangledName);
	return 0;
}

bool HookManager::WaitForModule(const char* moduleName, DWORD timeoutMs)
{
	const DWORD start = GetTickCount();

	while (GetModuleHandleA(moduleName) == nullptr)
	{
		if (GetTickCount() - start > timeoutMs)
			return false;

		Sleep(10);
	}

	return true;
}
