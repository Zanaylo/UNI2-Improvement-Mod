#include "Network/Steam/SteamNetwork.h"

#include "Network/NetLog.h"

#include <Windows.h>

#include <cstring>

namespace {

constexpr int kSendReliable = 2;

using Accessor_t = void*(__cdecl*)();
using SendP2PPacket_t = bool(__cdecl*)(void*, uint64_t, const void*, uint32_t, int, int);
using IsP2PPacketAvailable_t = bool(__cdecl*)(void*, uint32_t*, int);
using ReadP2PPacket_t = bool(__cdecl*)(void*, void*, uint32_t, uint32_t*, uint64_t*, int);

Accessor_t g_accessor = nullptr;
SendP2PPacket_t g_send = nullptr;
IsP2PPacketAvailable_t g_available = nullptr;
ReadP2PPacket_t g_read = nullptr;

volatile LONG g_ready = 0;
char g_status[128] = "not started";

void* Networking()
{
	void* networking = nullptr;

	__try
	{
		networking = g_accessor();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		networking = nullptr;
	}

	return networking;
}

bool Resolve()
{
	if (g_accessor != nullptr)
		return true;

	const HMODULE steam = GetModuleHandleA("steam_api.dll");

	if (steam == nullptr)
		return false;

	g_send = reinterpret_cast<SendP2PPacket_t>(GetProcAddress(steam, "SteamAPI_ISteamNetworking_SendP2PPacket"));
	g_available = reinterpret_cast<IsP2PPacketAvailable_t>(
		GetProcAddress(steam, "SteamAPI_ISteamNetworking_IsP2PPacketAvailable"));
	g_read = reinterpret_cast<ReadP2PPacket_t>(GetProcAddress(steam, "SteamAPI_ISteamNetworking_ReadP2PPacket"));

	const Accessor_t accessor = reinterpret_cast<Accessor_t>(GetProcAddress(steam, "SteamAPI_SteamNetworking_v006"));

	if (accessor == nullptr || g_send == nullptr || g_available == nullptr || g_read == nullptr)
	{
		strncpy_s(g_status, "steam_api.dll lacks the networking exports", _TRUNCATE);
		return false;
	}

	g_accessor = accessor;
	return true;
}

}

bool SteamNetwork::Initialize()
{
	if (g_ready != 0)
		return true;

	if (!Resolve() || Networking() == nullptr)
		return false;

	InterlockedExchange(&g_ready, 1);
	strncpy_s(g_status, "ready, mod channel open", _TRUNCATE);
	NetLog::Write("steam networking ready, mod channel %d", kChannel);
	return true;
}

bool SteamNetwork::IsReady()
{
	return g_ready != 0;
}

bool SteamNetwork::SendTo(uint64_t steamId, const void* data, int size)
{
	if (g_ready == 0 || steamId == 0 || data == nullptr || size <= 0)
		return false;

	void* const networking = Networking();

	if (networking == nullptr)
		return false;

	bool sent = false;

	__try
	{
		sent = g_send(networking, steamId, data, static_cast<uint32_t>(size), kSendReliable, kChannel);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		sent = false;
	}

	return sent;
}

bool SteamNetwork::Receive(void* buffer, int capacity, int& outSize, uint64_t& outPeer)
{
	outSize = 0;
	outPeer = 0;

	if (g_ready == 0 || buffer == nullptr || capacity <= 0)
		return false;

	void* const networking = Networking();

	if (networking == nullptr)
		return false;

	uint32_t size = 0;
	uint32_t got = 0;
	uint64_t from = 0;
	bool read = false;

	__try
	{
		read = g_available(networking, &size, kChannel) && size != 0 &&
			g_read(networking, buffer, static_cast<uint32_t>(capacity), &got, &from, kChannel);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		read = false;
	}

	if (!read)
		return false;

	outSize = static_cast<int>(got);
	outPeer = from;
	return true;
}

const char* SteamNetwork::GetStatusText()
{
	return g_status;
}
