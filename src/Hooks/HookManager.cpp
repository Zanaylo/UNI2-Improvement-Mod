#include "Hooks/HookManager.h"

#include "Core/Compat.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/DeviceHooks.h"

#include <MinHook.h>
#include <cstdio>
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

constexpr int kMaxJmpHops = 8;
constexpr int kMaxRecords = 96;

struct HookRecord
{
	void* requested;
	void* resolved;
	void* detour;
	const char* label;
	DWORD createdTick;
	bool reported;
};

HookRecord g_records[kMaxRecords] = {};
int g_recordCount = 0;

bool g_anyBroken = false;
char g_integrityStatus[192] = "no hooks checked yet";

HANDLE g_watchdogThread = nullptr;
HANDLE g_watchdogStop = nullptr;

bool IsExecutableAddress(const void* address)
{
	if (address == nullptr)
		return false;

	MEMORY_BASIC_INFORMATION mbi = {};
	if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0)
		return false;

	if (mbi.State != MEM_COMMIT)
		return false;

	const DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
		PAGE_EXECUTE_WRITECOPY;

	if ((mbi.Protect & executable) == 0)
		return false;

	return (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
}

bool ReadRelativeJump(const uint8_t* code, void*& outTarget)
{
	if (code[0] == 0xE9)
	{
		outTarget = const_cast<uint8_t*>(code) + 5 + *reinterpret_cast<const int32_t*>(code + 1);
		return true;
	}

	if (code[0] == 0xEB)
	{
		outTarget = const_cast<uint8_t*>(code) + 2 + *reinterpret_cast<const int8_t*>(code + 1);
		return true;
	}

	return false;
}

bool ReadIndirectJump(const uint8_t* code, void*& outTarget)
{
	if (code[0] != 0xFF || code[1] != 0x25)
		return false;

#ifdef _WIN64
	const uint8_t* const slot = code + 6 + *reinterpret_cast<const int32_t*>(code + 2);
#else
	const uint8_t* const slot = *reinterpret_cast<const uint8_t* const*>(code + 2);
#endif

	if (!IsReadableMemory(slot, sizeof(void*)))
		return false;

	outTarget = *reinterpret_cast<void* const*>(slot);
	return true;
}

bool ReadJumpTarget(const uint8_t* code, void*& outTarget)
{
	if (ReadRelativeJump(code, outTarget))
		return true;

	return ReadIndirectJump(code, outTarget);
}

HMODULE ModuleOf(const void* address)
{
	HMODULE owner = nullptr;
	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCSTR>(address), &owner))
	{
		return nullptr;
	}

	return owner;
}

bool LeavesTheModule(const void* from, const void* to)
{
	const HMODULE fromModule = ModuleOf(from);
	if (fromModule == nullptr)
		return true;

	return ModuleOf(to) != fromModule;
}

bool AddressIsInModModule(const void* address)
{
	return ModuleOf(address) == GetModModuleHandle();
}

bool NextJumpTarget(void* current, bool foreignJumpsOnly, void*& outNext)
{
	if (!IsExecutableAddress(current) || !IsReadableMemory(current, 8))
		return false;

	void* next = nullptr;
	if (!ReadJumpTarget(reinterpret_cast<const uint8_t*>(current), next))
		return false;

	if (next == nullptr || next == current || !IsExecutableAddress(next))
		return false;

	if (foreignJumpsOnly && !LeavesTheModule(current, next))
		return false;

	outNext = next;
	return true;
}

void* WalkJmpChain(void* target, bool foreignJumpsOnly, int* outHops)
{
	if (outHops != nullptr)
		*outHops = 0;

	if (target == nullptr)
		return nullptr;

	void* current = target;

	for (int hop = 0; hop < kMaxJmpHops; ++hop)
	{
		void* next = nullptr;
		if (!NextJumpTarget(current, foreignJumpsOnly, next))
			break;

		current = next;

		if (outHops != nullptr)
			*outHops = hop + 1;
	}

	return current;
}

HookRecord* FindRecordByRequested(const void* requested)
{
	for (int i = 0; i < g_recordCount; ++i)
	{
		if (g_records[i].requested == requested)
			return &g_records[i];
	}

	return nullptr;
}

HookRecord* FindRecordByResolved(const void* resolved)
{
	for (int i = 0; i < g_recordCount; ++i)
	{
		if (g_records[i].resolved == resolved)
			return &g_records[i];
	}

	return nullptr;
}

void RememberHook(void* requested, void* resolved, void* detour, const char* label)
{
	if (g_recordCount >= kMaxRecords)
		return;

	HookRecord& record = g_records[g_recordCount++];
	record.requested = requested;
	record.resolved = resolved;
	record.detour = detour;
	record.label = label;
	record.createdTick = GetTickCount();
	record.reported = false;
}

void* ResolvedAddressFor(void* target)
{
	const HookRecord* const record = FindRecordByRequested(target);
	if (record != nullptr)
		return record->resolved;

	return HookManager::SkipJmpChain(target);
}

const char* RtssAdvice()
{
	if (!Compat::IsRtssPresent())
		return "";

	return " RTSS is running: turn on 'Use Microsoft Detours API hooking' in its Settings / General"
		" / Injection properties, or set this game's RTSS profile Application detection level to"
		" None.";
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
	StopIntegrityWatchdog();

	if (!g_initialized)
		return;

	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
	g_initialized = false;
}

void* HookManager::SkipJmpChain(void* target, int* outHops)
{
	return WalkJmpChain(target, true, outHops);
}

namespace {

bool HookTargetIsAvailable(void* requested, void* resolved, const char* label)
{
	if (AddressIsInModModule(resolved))
	{
		LOG("CreateHook '%s' refused: 0x%p already ends in this mod's own handler", label, requested);
		return false;
	}

	const HookRecord* const taken = FindRecordByResolved(resolved);
	if (taken == nullptr)
		return true;

	LOG("CreateHook '%s' refused: 0x%p is the same code as '%s'", label, resolved, taken->label);
	return false;
}

void LogHookCreated(const char* label, void* requested, void* resolved, int hops)
{
	if (hops == 0)
	{
		LOG("Hook created: %s at 0x%p", label, resolved);
		return;
	}

	LOG("Hook created: %s at 0x%p (%d jump%s past 0x%p - another hook engine is on this function, "
		"so the mod went in behind it)", label, resolved, hops, hops == 1 ? "" : "s", requested);
}

}

bool HookManager::CreateHook(void* target, void* detour, void** original, const char* label)
{
	if (target == nullptr)
	{
		LOG("CreateHook '%s' failed: null target", label);
		return false;
	}

	int hops = 0;
	void* const resolved = SkipJmpChain(target, &hops);

	if (!HookTargetIsAvailable(target, resolved, label))
		return false;

	const MH_STATUS status = MH_CreateHook(resolved, detour, original);
	if (status != MH_OK)
	{
		LOG("MH_CreateHook '%s' failed: %d", label, status);
		return false;
	}

	RememberHook(target, resolved, detour, label);
	LogHookCreated(label, target, resolved, hops);
	return true;
}

bool HookManager::CreateAndEnableHook(void* target, void* detour, void** original, const char* label)
{
	if (!CreateHook(target, detour, original, label))
		return false;

	const MH_STATUS status = MH_EnableHook(ResolvedAddressFor(target));
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
	void* const resolved = ResolvedAddressFor(target);

	const MH_STATUS status = enabled ? MH_EnableHook(resolved) : MH_DisableHook(resolved);
	if (status != MH_OK)
	{
		LOG("MH_%sHook failed on 0x%p: %d", enabled ? "Enable" : "Disable", target, status);
		return false;
	}

	return true;
}

namespace {

enum class HookState
{
	Live,
	Wrapped,
	Removed
};

HookState ClassifyHook(const HookRecord& record, int& outHops, const void*& outEndpoint)
{
	outEndpoint = WalkJmpChain(record.resolved, false, &outHops);

	if (outEndpoint == record.detour)
		return HookState::Live;

	if (outHops == 0)
		return HookState::Removed;

	return HookState::Wrapped;
}

void ReportRemoved(const HookRecord& record)
{
	LOG("Hook integrity: '%s' at 0x%p was restored to its original code. The mod is no longer "
		"called there.%s", record.label, record.resolved, RtssAdvice());
}

void ReportWrapped(const HookRecord& record, int hops, const void* endpoint)
{
	LOG("Hook integrity: another engine now sits in front of '%s' at 0x%p (%d jump%s to 0x%p). The "
		"mod is still behind it in the chain.", record.label, record.resolved, hops,
		hops == 1 ? "" : "s", endpoint);
}

void UpdateIntegrityStatus(int removed)
{
	const char* const plural = g_recordCount == 1 ? "" : "s";

	if (removed == 0)
	{
		sprintf_s(g_integrityStatus, "%d hook%s, none removed", g_recordCount, plural);
		return;
	}

	sprintf_s(g_integrityStatus, "%d of %d hook%s restored away by another engine", removed,
		g_recordCount, plural);
}

bool IsTooYoungToVerify(const HookRecord& record)
{
	constexpr DWORD kSettleMs = 2000;
	return GetTickCount() - record.createdTick < kSettleMs;
}

}

int HookManager::VerifyHooks()
{
	int removed = 0;

	for (int i = 0; i < g_recordCount; ++i)
	{
		HookRecord& record = g_records[i];

		if (IsTooYoungToVerify(record))
			continue;

		int hops = 0;
		const void* endpoint = nullptr;
		const HookState state = ClassifyHook(record, hops, endpoint);

		if (state == HookState::Live)
		{
			record.reported = false;
			continue;
		}

		if (state == HookState::Removed)
		{
			++removed;
			g_anyBroken = true;
		}

		if (record.reported)
			continue;

		record.reported = true;

		if (state == HookState::Removed)
		{
			ReportRemoved(record);
			continue;
		}

		ReportWrapped(record, hops, endpoint);
	}

	UpdateIntegrityStatus(removed);
	return removed;
}

namespace {

constexpr DWORD kWatchdogIntervalMs = 3000;
constexpr int kStalledChecksBeforeReport = 4;

void ReportPresentStall(int stalledChecks)
{
	LOG("Hook integrity: the device is up but the mod's Present has not been called for %d seconds. "
		"Nothing the mod draws is reaching the screen.%s",
		static_cast<int>((stalledChecks * kWatchdogIntervalMs) / 1000), RtssAdvice());
}

class PresentStallDetector
{
public:
	void Check()
	{
		if (!DeviceHooks::IsInstalled())
			return;

		const unsigned long present = DeviceHooks::PresentCount();

		if (present != m_lastPresent)
		{
			m_lastPresent = present;
			m_stalledChecks = 0;
			m_reported = false;
			return;
		}

		if (++m_stalledChecks < kStalledChecksBeforeReport || m_reported)
			return;

		m_reported = true;
		g_anyBroken = true;
		ReportPresentStall(m_stalledChecks);
	}

private:
	unsigned long m_lastPresent = 0;
	int m_stalledChecks = 0;
	bool m_reported = false;
};

DWORD WINAPI IntegrityWatchdog(LPVOID)
{
	PresentStallDetector stallDetector;

	while (WaitForSingleObject(g_watchdogStop, kWatchdogIntervalMs) == WAIT_TIMEOUT)
	{
		HookManager::VerifyHooks();
		stallDetector.Check();
	}

	return 0;
}

}

void HookManager::StartIntegrityWatchdog()
{
	if (g_watchdogThread != nullptr)
		return;

	g_watchdogStop = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	if (g_watchdogStop == nullptr)
		return;

	g_watchdogThread = CreateThread(nullptr, 0, IntegrityWatchdog, nullptr, 0, nullptr);
	if (g_watchdogThread == nullptr)
	{
		CloseHandle(g_watchdogStop);
		g_watchdogStop = nullptr;
	}
}

void HookManager::StopIntegrityWatchdog()
{
	if (g_watchdogStop != nullptr)
		SetEvent(g_watchdogStop);

	if (g_watchdogThread != nullptr)
	{
		constexpr DWORD kStopTimeoutMs = 250;

		WaitForSingleObject(g_watchdogThread, kStopTimeoutMs);
		CloseHandle(g_watchdogThread);
		g_watchdogThread = nullptr;
	}

	if (g_watchdogStop != nullptr)
	{
		CloseHandle(g_watchdogStop);
		g_watchdogStop = nullptr;
	}
}

bool HookManager::AnyHookBroken()
{
	return g_anyBroken;
}

const char* HookManager::IntegrityStatus()
{
	return g_integrityStatus;
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
