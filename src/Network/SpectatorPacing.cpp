#include "Network/SpectatorPacing.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

namespace {

typedef void(__fastcall* SendPendingFn)(void*);

constexpr DWORD kSendEveryMs = 50;

SendPendingFn oSendPending = nullptr;

uintptr_t g_session = 0;
uint32_t g_paced = 0;
DWORD g_sentAt[GameOffsets::kGgpoMostSpectators] = {};

int PacedIndex(uintptr_t endpoint)
{
	const uintptr_t first = g_session + GameOffsets::kGgpoSpectatorEndpoints;

	if (g_paced == 0 || endpoint < first)
		return -1;

	const uintptr_t offset = endpoint - first;
	const uintptr_t index = offset / GameOffsets::kGgpoEndpointStride;

	if (offset % GameOffsets::kGgpoEndpointStride != 0 || index >= static_cast<uintptr_t>(GameOffsets::kGgpoMostSpectators))
		return -1;

	return (g_paced >> index) & 1 ? static_cast<int>(index) : -1;
}

void __fastcall HookedSendPending(void* endpoint)
{
	const int index = PacedIndex(reinterpret_cast<uintptr_t>(endpoint));

	if (index < 0)
	{
		oSendPending(endpoint);
		return;
	}

	const uint32_t state = *reinterpret_cast<const uint32_t*>(reinterpret_cast<uintptr_t>(endpoint) + GameOffsets::kGgpoEndpointState);
	const DWORD now = GetTickCount();

	if (state == GameOffsets::kGgpoStateRunning && now - g_sentAt[index] < kSendEveryMs)
		return;

	g_sentAt[index] = now;
	oSendPending(endpoint);
}

}

bool SpectatorPacing::Install()
{
	if (oSendPending != nullptr)
		return true;

	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnSendPendingOutput));

	if (IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)) &&
		HookManager::CreateAndEnableHook(target, &HookedSendPending, reinterpret_cast<void**>(&oSendPending), "SendPendingOutput"))
	{
		return true;
	}

	oSendPending = nullptr;
	LOG("SpectatorPacing: the input sender is not where this game version expects it");
	return false;
}

void SpectatorPacing::Reset()
{
	g_paced = 0;
	g_session = 0;
}

void SpectatorPacing::Pace(uintptr_t session, int index)
{
	if (session == 0 || index < 0 || index >= GameOffsets::kGgpoMostSpectators)
		return;

	if (session != g_session)
		g_paced = 0;

	g_session = session;
	g_sentAt[index] = 0;
	g_paced |= 1u << index;
}
