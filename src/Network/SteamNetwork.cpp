#include "Network/SteamNetwork.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Hooks/HookManager.h"

#include <MinHook.h>
#include <Windows.h>

#include <cstdio>

namespace {

constexpr int kSendP2PPacket = 0;
constexpr int kIsP2PPacketAvailable = 1;
constexpr int kReadP2PPacket = 2;
constexpr int kAcceptP2PSession = 3;

constexpr int kSendReliable = 2;

constexpr int kGetISteamGenericInterface = 12;

using SendP2PPacket_t = bool(__fastcall*)(void*, void*, uint64_t, const void*, uint32_t, int, int);
using IsP2PPacketAvailable_t = bool(__fastcall*)(void*, void*, uint32_t*, int);
using ReadP2PPacket_t = bool(__fastcall*)(void*, void*, void*, uint32_t, uint32_t*, uint64_t*, int);
using AcceptP2PSession_t = bool(__fastcall*)(void*, void*, uint64_t);
using GetGenericInterface_t = void*(__fastcall*)(void*, void*, int32_t, int32_t, const char*);

using GetHSteamUser_t = int32_t(__cdecl*)();
using GetHSteamPipe_t = int32_t(__cdecl*)();
using SteamClient_t = void*(__cdecl*)();

void* g_networking = nullptr;
bool g_ready = false;
bool g_sendHooked = false;
uint64_t g_peer = 0;
uint64_t g_lastLoggedPeer = 0;

volatile DWORD g_lastTrafficTick = 0;
char g_status[192] = "not started";

SendP2PPacket_t oSendP2PPacket = nullptr;

void* VTableEntry(void* object, int index)
{
	if (object == nullptr)
		return nullptr;

	void** vtable = *reinterpret_cast<void***>(object);
	if (!IsReadableMemory(vtable, sizeof(void*) * (index + 1)))
		return nullptr;

	return vtable[index];
}

bool __fastcall HookedSendP2PPacket(void* self, void* edx, uint64_t steamIDRemote, const void* data,
	uint32_t size, int sendType, int channel)
{
	if (steamIDRemote != 0 && channel != SteamNetwork::kChannel)
	{
		g_peer = steamIDRemote;
		g_lastTrafficTick = GetTickCount();
	}

	return oSendP2PPacket(self, edx, steamIDRemote, data, size, sendType, channel);
}

void HookSend()
{
	void* target = VTableEntry(g_networking, kSendP2PPacket);
	if (target == nullptr)
		return;

	g_sendHooked = HookManager::CreateAndEnableHook(target, &HookedSendP2PPacket,
		reinterpret_cast<void**>(&oSendP2PPacket), "ISteamNetworking::SendP2PPacket");
}

}

bool SteamNetwork::Initialize()
{
	if (g_ready)
		return true;

	HMODULE steam = GetModuleHandleA("steam_api.dll");
	if (steam == nullptr)
	{
		strncpy_s(g_status, "steam_api.dll is not loaded", _TRUNCATE);
		LOG("SteamNetwork: %s", g_status);
		return false;
	}

	auto getUser = reinterpret_cast<GetHSteamUser_t>(
		GetProcAddress(steam, "SteamAPI_GetHSteamUser"));
	auto getPipe = reinterpret_cast<GetHSteamPipe_t>(
		GetProcAddress(steam, "SteamAPI_GetHSteamPipe"));
	auto steamClient = reinterpret_cast<SteamClient_t>(GetProcAddress(steam, "SteamClient"));

	if (getUser == nullptr || getPipe == nullptr || steamClient == nullptr)
	{
		strncpy_s(g_status, "steam_api.dll is missing the exports this needs", _TRUNCATE);
		LOG("SteamNetwork: %s", g_status);
		return false;
	}

	void* client = nullptr;
	int32_t user = 0;
	int32_t pipe = 0;

	__try
	{
		client = steamClient();
		user = getUser();
		pipe = getPipe();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		client = nullptr;
	}

	if (client == nullptr || pipe == 0)
	{
		strncpy_s(g_status, "no Steam client yet", _TRUNCATE);
		return false;
	}

	auto generic = reinterpret_cast<GetGenericInterface_t>(
		VTableEntry(client, kGetISteamGenericInterface));

	if (generic == nullptr)
	{
		strncpy_s(g_status, "ISteamClient has no readable vtable", _TRUNCATE);
		LOG("SteamNetwork: %s", g_status);
		return false;
	}

	void* networking = nullptr;

	__try
	{
		networking = generic(client, nullptr, user, pipe, "SteamNetworking006");
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		networking = nullptr;
	}

	if (networking == nullptr || VTableEntry(networking, kReadP2PPacket) == nullptr)
	{
		strncpy_s(g_status, "SteamNetworking006 was not handed over", _TRUNCATE);
		LOG("SteamNetwork: %s", g_status);
		return false;
	}

	g_networking = networking;
	g_ready = true;

	HookSend();

	sprintf_s(g_status, "SteamNetworking006 at 0x%p, channel %d", networking, kChannel);
	LOG("SteamNetwork: %s", g_status);
	return true;
}

bool SteamNetwork::IsReady()
{
	return g_ready;
}

bool SteamNetwork::CanSeePeerTraffic()
{
	return g_sendHooked;
}

uint64_t SteamNetwork::GetPeer()
{
	return g_peer;
}

bool SteamNetwork::HasPeer()
{
	if (g_peer != 0 && g_peer != g_lastLoggedPeer)
	{
		g_lastLoggedPeer = g_peer;
		LOG("SteamNetwork: peer is %llu", (unsigned long long)g_peer);

		auto accept = reinterpret_cast<AcceptP2PSession_t>(
			VTableEntry(g_networking, kAcceptP2PSession));

		if (accept != nullptr)
			accept(g_networking, nullptr, g_peer);
	}

	return g_peer != 0;
}

bool SteamNetwork::HasRecentPeerTraffic(unsigned withinMs)
{
	const DWORD last = g_lastTrafficTick;
	if (last == 0)
		return false;

	return GetTickCount() - last < withinMs;
}

bool SteamNetwork::Send(const void* data, int size)
{
	if (!g_ready || g_peer == 0 || data == nullptr || size <= 0)
		return false;

	auto send = reinterpret_cast<SendP2PPacket_t>(VTableEntry(g_networking, kSendP2PPacket));
	if (send == nullptr)
		return false;

	return send(g_networking, nullptr, g_peer, data, static_cast<uint32_t>(size), kSendReliable,
		kChannel);
}

bool SteamNetwork::Receive(void* buffer, int capacity, int& outSize, uint64_t& outPeer)
{
	outSize = 0;
	outPeer = 0;

	if (!g_ready || buffer == nullptr || capacity <= 0)
		return false;

	auto available = reinterpret_cast<IsP2PPacketAvailable_t>(
		VTableEntry(g_networking, kIsP2PPacketAvailable));
	auto read = reinterpret_cast<ReadP2PPacket_t>(VTableEntry(g_networking, kReadP2PPacket));

	if (available == nullptr || read == nullptr)
		return false;

	uint32_t size = 0;
	if (!available(g_networking, nullptr, &size, kChannel) || size == 0)
		return false;

	uint32_t got = 0;
	uint64_t from = 0;

	if (!read(g_networking, nullptr, buffer, static_cast<uint32_t>(capacity), &got, &from, kChannel))
		return false;

	outSize = static_cast<int>(got);
	outPeer = from;
	return true;
}

const char* SteamNetwork::GetStatusText()
{
	return g_status;
}
