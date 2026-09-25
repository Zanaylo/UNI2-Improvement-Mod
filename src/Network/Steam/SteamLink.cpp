#include "Network/Steam/SteamLink.h"

#include "Network/NetLog.h"

#include <Windows.h>

#include <cstring>

namespace {

constexpr int kIdentitySteamId = 16;

#pragma pack(push, 8)
struct P2PSessionState
{
	uint8_t connectionActive;
	uint8_t connecting;
	uint8_t sessionError;
	uint8_t usingRelay;
	int32_t bytesQueued;
	int32_t packetsQueued;
	uint32_t remoteIp;
	uint16_t remotePort;
};

struct RealTimeStatus
{
	int32_t state;
	int32_t ping;
	float qualityLocal;
	float qualityRemote;
	float outPacketsPerSec;
	float outBytesPerSec;
	float inPacketsPerSec;
	float inBytesPerSec;
	int32_t sendRateBytesPerSecond;
	int32_t pendingUnreliable;
	int32_t pendingReliable;
	int32_t sentUnackedReliable;
	int64_t queueMicros;
	uint32_t reserved[16];
};

struct RelayStatus
{
	int32_t availability;
	int32_t pingMeasurementInProgress;
	int32_t availabilityNetworkConfig;
	int32_t availabilityAnyRelay;
	char debug[256];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Identity
{
	int32_t type;
	int32_t size;
	uint64_t steamId;
	uint8_t rest[120];
};
#pragma pack(pop)

using Accessor_t = void*(__cdecl*)();
using GetP2PSessionState_t = bool(__cdecl*)(void*, uint64_t, P2PSessionState*);
using GetSessionConnectionInfo_t = int(__cdecl*)(void*, const Identity*, uint8_t*, RealTimeStatus*);
using GetRelayNetworkStatus_t = int(__cdecl*)(void*, RelayStatus*);

bool g_resolved = false;
Accessor_t g_networking = nullptr;
Accessor_t g_messages = nullptr;
Accessor_t g_utils = nullptr;
GetP2PSessionState_t g_p2pState = nullptr;
GetSessionConnectionInfo_t g_messagesInfo = nullptr;
GetRelayNetworkStatus_t g_relay = nullptr;

SRWLOCK g_lock = SRWLOCK_INIT;
SteamLink::Sample g_sample = {};

void Resolve()
{
	if (g_resolved)
		return;

	const HMODULE steam = GetModuleHandleA("steam_api.dll");

	if (steam == nullptr)
		return;

	g_resolved = true;
	g_networking = reinterpret_cast<Accessor_t>(GetProcAddress(steam, "SteamAPI_SteamNetworking_v006"));
	g_messages = reinterpret_cast<Accessor_t>(GetProcAddress(steam, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002"));
	g_utils = reinterpret_cast<Accessor_t>(GetProcAddress(steam, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004"));
	g_p2pState = reinterpret_cast<GetP2PSessionState_t>(GetProcAddress(steam, "SteamAPI_ISteamNetworking_GetP2PSessionState"));
	g_messagesInfo = reinterpret_cast<GetSessionConnectionInfo_t>(
		GetProcAddress(steam, "SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo"));
	g_relay = reinterpret_cast<GetRelayNetworkStatus_t>(
		GetProcAddress(steam, "SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus"));
}

void* Call(Accessor_t accessor)
{
	if (accessor == nullptr)
		return nullptr;

	void* result = nullptr;

	__try
	{
		result = accessor();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		result = nullptr;
	}

	return result;
}

bool ReadP2P(uint64_t peer, P2PSessionState& out)
{
	void* const networking = Call(g_networking);

	if (networking == nullptr || g_p2pState == nullptr)
		return false;

	bool read = false;

	__try
	{
		read = g_p2pState(networking, peer, &out);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		read = false;
	}

	return read;
}

bool ReadMessages(uint64_t peer, int& state, RealTimeStatus& out)
{
	void* const messages = Call(g_messages);

	if (messages == nullptr || g_messagesInfo == nullptr)
		return false;

	Identity identity = {};
	identity.type = kIdentitySteamId;
	identity.size = sizeof(uint64_t);
	identity.steamId = peer;

	uint8_t info[768] = {};

	__try
	{
		state = g_messagesInfo(messages, &identity, info, &out);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

int ReadRelay()
{
	void* const utils = Call(g_utils);

	if (utils == nullptr || g_relay == nullptr)
		return -1;

	RelayStatus status = {};
	int availability = -1;

	__try
	{
		availability = g_relay(utils, &status);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		availability = -1;
	}

	return availability;
}

}

void SteamLink::Measure(uint64_t peer)
{
	Resolve();

	Sample sample = {};
	sample.peer = peer;
	sample.relayAvailability = ReadRelay();

	P2PSessionState p2p = {};
	sample.p2pRead = peer != 0 && ReadP2P(peer, p2p);
	sample.connectionActive = p2p.connectionActive != 0;
	sample.connecting = p2p.connecting != 0;
	sample.sessionError = p2p.sessionError;
	sample.usingRelay = p2p.usingRelay != 0;
	sample.bytesQueued = p2p.bytesQueued;
	sample.packetsQueued = p2p.packetsQueued;

	RealTimeStatus status = {};
	sample.messagesRead = peer != 0 && ReadMessages(peer, sample.messagesState, status);
	sample.ping = status.ping;
	sample.qualityLocal = status.qualityLocal;
	sample.qualityRemote = status.qualityRemote;
	sample.outPacketsPerSec = status.outPacketsPerSec;
	sample.inPacketsPerSec = status.inPacketsPerSec;
	sample.outBytesPerSec = status.outBytesPerSec;
	sample.inBytesPerSec = status.inBytesPerSec;
	sample.pendingReliable = status.pendingReliable;
	sample.pendingUnreliable = status.pendingUnreliable;
	sample.queueMicros = status.queueMicros;
	sample.valid = true;

	AcquireSRWLockExclusive(&g_lock);
	g_sample = sample;
	ReleaseSRWLockExclusive(&g_lock);

	if (peer == 0)
		return;

	NetLog::Write("steam %llu p2p %s%s, relay %d, queued %d B %d pkt, error %d | messages state %d ping %d "
		"quality %.2f/%.2f out %.0f in %.0f pkt/s pending %d/%d | relay network %s",
		static_cast<unsigned long long>(peer), sample.p2pRead ? (sample.connectionActive ? "active" : "inactive") : "unread",
		sample.connecting ? " connecting" : "", sample.usingRelay ? 1 : 0, sample.bytesQueued, sample.packetsQueued,
		sample.sessionError, sample.messagesState, sample.ping, sample.qualityLocal, sample.qualityRemote,
		sample.outPacketsPerSec, sample.inPacketsPerSec, sample.pendingReliable, sample.pendingUnreliable,
		AvailabilityName(sample.relayAvailability));
}

void SteamLink::Take(Sample& out)
{
	AcquireSRWLockShared(&g_lock);
	out = g_sample;
	ReleaseSRWLockShared(&g_lock);
}

const char* SteamLink::AvailabilityName(int availability)
{
	switch (availability)
	{
	case 100:
		return "current";
	case 3:
		return "attempting";
	case 2:
		return "waiting";
	case 1:
		return "never tried";
	case 0:
		return "unknown";
	case -10:
		return "retrying";
	case -100:
		return "previously worked, now failed";
	case -101:
		return "failed";
	case -102:
		return "cannot try";
	default:
		return "unread";
	}
}
