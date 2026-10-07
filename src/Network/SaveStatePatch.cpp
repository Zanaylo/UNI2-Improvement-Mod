#include "Network/SaveStatePatch.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/IndirectCall.h"
#include "Network/SaveStatePool.h"

#include <Windows.h>

namespace {

using MallocFn = void*(__cdecl*)(size_t);
using FreeFn = void(__cdecl*)(void*);

constexpr int kPoolSlots = 24;
constexpr size_t kBytesPerKilobyte = 1024;

class VirtualPages : public IPageSource
{
public:
	void* Reserve(size_t bytes) override
	{
		return VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	}

	void Release(void* pages) override
	{
		VirtualFree(pages, 0, MEM_RELEASE);
	}
};

struct CallSite
{
	uint8_t* code;
	void* target;
};

VirtualPages g_pages;
SaveStatePool g_pool(g_pages, GameOffsets::kGgpoSaveStateBytes, kPoolSlots);
SRWLOCK g_lock = SRWLOCK_INIT;

MallocFn g_gameMalloc = nullptr;
FreeFn g_gameFree = nullptr;
volatile LONG g_firstServed = 0;
bool g_installed = false;

void LogFirstServed()
{
	if (InterlockedExchange(&g_firstServed, 1) == 0)
		LOG("SaveStatePool: the first GGPO save state came from the pool");
}

void LogDrained(const SaveStatePool::Counters& counters)
{
	LOG("SaveStatePool: GGPO session over, %d save(s) served from %d buffer(s), peak %d in use, %d left to the "
		"game's malloc", counters.served, counters.created, counters.peakBusy, counters.fellBack);
}

void* __cdecl PooledMalloc(size_t bytes)
{
	AcquireSRWLockExclusive(&g_lock);
	void* const buffer = g_pool.Take(bytes);
	ReleaseSRWLockExclusive(&g_lock);

	if (buffer == nullptr)
		return g_gameMalloc(bytes);

	LogFirstServed();
	return buffer;
}

void __cdecl PooledFree(void* buffer)
{
	AcquireSRWLockExclusive(&g_lock);
	const SaveStatePool::Returned returned = g_pool.Give(buffer);
	const SaveStatePool::Counters counters = g_pool.GetCounters();

	if (returned == SaveStatePool::Returned::Drained)
		g_pool.ResetCounters();

	ReleaseSRWLockExclusive(&g_lock);

	if (returned == SaveStatePool::Returned::NotOurs)
	{
		g_gameFree(buffer);
		return;
	}

	if (returned == SaveStatePool::Returned::Drained)
		LogDrained(counters);
}

void* g_pooledMallocEntry = reinterpret_cast<void*>(&PooledMalloc);
void* g_pooledFreeEntry = reinterpret_cast<void*>(&PooledFree);

bool IsCrtExport(void* target, const char* name)
{
	HMODULE owner = nullptr;

	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		static_cast<LPCSTR>(target), &owner) || owner == nullptr)
	{
		return false;
	}

	return reinterpret_cast<void*>(GetProcAddress(owner, name)) == target;
}

bool ResolveCallSite(uintptr_t functionRva, uintptr_t callOffset, const char* crtName, CallSite& out)
{
	const uintptr_t function = CodeSignatures::Address(functionRva);
	if (!IsAddressInGameModule(function))
		return false;

	uint8_t* const code = reinterpret_cast<uint8_t*>(function + callOffset);
	uint8_t bytes[IndirectCall::kLength] = {};
	uint32_t slot = 0;

	if (!TryReadMemory(bytes, code, sizeof(bytes)) || !IndirectCall::Decode(bytes, slot) ||
		!IsAddressInGameModule(slot))
	{
		return false;
	}

	void* target = nullptr;
	if (!TryReadMemory(&target, reinterpret_cast<const void*>(static_cast<uintptr_t>(slot)), sizeof(target)) ||
		!IsCrtExport(target, crtName))
	{
		return false;
	}

	out = { code, target };
	return true;
}

bool Redirect(const CallSite& site, void** entry)
{
	const uint32_t operand = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(entry));

	return WriteCodeBytes(site.code + IndirectCall::kOperandOffset, &operand, sizeof(operand));
}

}

bool SaveStatePatch::Install()
{
	if (g_installed)
		return true;

	if (!g_modVals.saveStatePool)
	{
		LOG("SaveStatePool: off in the ini, GGPO save states use the game's own malloc");
		return false;
	}

	CallSite allocation = {};
	CallSite release = {};

	if (!ResolveCallSite(GameOffsets::kFnGgpoSaveState, GameOffsets::kGgpoSaveStateMallocCall, "malloc", allocation) ||
		!ResolveCallSite(GameOffsets::kFnGgpoFreeBuffer, GameOffsets::kGgpoFreeBufferFreeCall, "free", release))
	{
		LOG("SaveStatePool: the GGPO save and free calls are not the measured malloc and free, left alone");
		return false;
	}

	g_gameMalloc = reinterpret_cast<MallocFn>(allocation.target);
	g_gameFree = reinterpret_cast<FreeFn>(release.target);

	if (!Redirect(release, &g_pooledFreeEntry))
	{
		LOG("SaveStatePool: could not redirect the GGPO free call, left alone");
		return false;
	}

	if (!Redirect(allocation, &g_pooledMallocEntry))
	{
		LOG("SaveStatePool: could not redirect the GGPO save allocation, saves keep the game's malloc");
		return false;
	}

	g_installed = true;
	LOG("SaveStatePool: GGPO save states reuse up to %d buffer(s) of %u KB instead of a malloc and free per frame",
		kPoolSlots, static_cast<unsigned>(GameOffsets::kGgpoSaveStateBytes / kBytesPerKilobyte));
	return true;
}

bool SaveStatePatch::IsInstalled()
{
	return g_installed;
}
