#include "Network/NetLink.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/GgpoLogCapture.h"
#include "Network/NetLog.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kIndividualHigh = 0x01100001;
constexpr DWORD kSummaryMs = 1000;
constexpr int64_t kHitchMicros = 50000;
constexpr int64_t kSlowMicros = 20000;
constexpr int kHitchesPerSecond = 10;
constexpr int64_t kHeavyModMicros = 4000;

struct Window
{
	DWORD start;
	int frames;
	int64_t intervalSum;
	int64_t intervalMax;
	int slow;
	int64_t modSum;
	int64_t modMax;
	int startFrame;
	int startRollbacks;
	int maxPending;
	int hitches;
};

NetLink::Snapshot g_now = {};
NetLink::Snapshot g_shared = {};
SRWLOCK g_lock = SRWLOCK_INIT;

LARGE_INTEGER g_frequency = {};
LARGE_INTEGER g_lastPresent = {};
Window g_window = {};

DWORD g_syncStart = 0;
bool g_blindLogged = false;

uint32_t ReadDword(uintptr_t address)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(address), value);

	return value;
}

bool ReadDwordChecked(uintptr_t address, uint32_t& out)
{
	return TryReadDword(reinterpret_cast<const void*>(address), out);
}

uint8_t ReadByte(uintptr_t address)
{
	uint8_t value = 0;
	TryReadMemory(&value, reinterpret_cast<const void*>(address), sizeof(value));

	return value;
}

uint64_t ReadSteamId(uintptr_t endpoint)
{
	const uint32_t low = ReadDword(endpoint + GameOffsets::kGgpoEndpointSteamLow);
	const uint32_t high = ReadDword(endpoint + GameOffsets::kGgpoEndpointSteamHigh);

	if (high != kIndividualHigh || low == 0)
		return 0;

	return (static_cast<uint64_t>(high) << 32) | low;
}

bool ReadEndpoint(uintptr_t endpoint, NetLink::Endpoint& out)
{
	if (ReadDword(endpoint + GameOffsets::kGgpoEndpointUdp) == 0)
		return false;

	const uint64_t id = ReadSteamId(endpoint);

	if (id == 0)
		return false;

	out.id = id;
	out.state = ReadDword(endpoint + GameOffsets::kGgpoEndpointState);
	out.pending = static_cast<int>(ReadDword(endpoint + GameOffsets::kGgpoEndpointPendingOutput));
	out.ping = static_cast<int>(ReadDword(endpoint + GameOffsets::kGgpoEndpointRoundTrip));
	out.kbps = static_cast<int>(ReadDword(endpoint + GameOffsets::kGgpoEndpointKbpsSent));
	out.localBehind = static_cast<int>(ReadDword(endpoint + GameOffsets::kGgpoEndpointLocalBehind));
	out.remoteBehind = static_cast<int>(ReadDword(endpoint + GameOffsets::kGgpoEndpointRemoteBehind));
	return true;
}

void ReadPlayers(uintptr_t session, NetLink::Snapshot& out)
{
	uint32_t players = 0;
	uint32_t spectators = 0;
	uint32_t endpoints = 0;

	const bool read = ReadDwordChecked(session + GameOffsets::kGgpoPlayerCount, players) &&
		ReadDwordChecked(session + GameOffsets::kGgpoSpectatorCount, spectators) &&
		ReadDwordChecked(session + GameOffsets::kGgpoPlayerEndpoints, endpoints);

	out.players = static_cast<int>(players);
	out.spectators = static_cast<int>(spectators);
	out.synchronizing = ReadByte(session + GameOffsets::kGgpoSynchronizing) != 0;
	out.layoutValid = read && players >= 1 && players <= static_cast<uint32_t>(GameOffsets::kGgpoMostPlayers) &&
		spectators <= static_cast<uint32_t>(GameOffsets::kGgpoMostSpectators) && endpoints >= 0x10000;

	if (!out.layoutValid)
		return;

	for (uint32_t i = 0; i < players; ++i)
	{
		NetLink::Endpoint endpoint = {};

		if (!ReadEndpoint(endpoints + i * GameOffsets::kGgpoEndpointStride, endpoint))
			continue;

		out.peer = endpoint;
		out.hasPeer = true;
		return;
	}
}

void ReadSpectator(uintptr_t session, NetLink::Snapshot& out)
{
	out.layoutValid = true;
	out.hasPeer = ReadEndpoint(session + GameOffsets::kGgpoSpectatorHostEndpoint, out.peer);
}

NetLink::Backend BackendOf(uint32_t vtable)
{
	if (vtable == 0)
		return NetLink::Backend_None;

	if (vtable == RvaToAddress(GameOffsets::kGgpoBackendVTable))
		return NetLink::Backend_Players;

	if (vtable == RvaToAddress(GameOffsets::kGgpoSpectatorBackendVTable))
		return NetLink::Backend_Spectator;

	return NetLink::Backend_Unknown;
}

uint64_t ReadLobby()
{
	const uintptr_t address = RvaToAddress(GameOffsets::kSessionManagerLobbyId);
	uint32_t low = 0;
	uint32_t high = 0;

	if (address == 0 || !TryReadUnaligned(reinterpret_cast<const void*>(address), low) ||
		!TryReadUnaligned(reinterpret_cast<const void*>(address + 4), high))
	{
		return 0;
	}

	const uint64_t id = (static_cast<uint64_t>(high) << 32) | low;
	const bool lobby = ((id >> 56) & 0xff) == 1 && ((id >> 52) & 0x0f) == 8;

	return lobby ? id : 0;
}

NetLink::Snapshot Read(const NetLink::Snapshot& previous)
{
	NetLink::Snapshot out = {};
	out.tick = GetTickCount();
	out.lastPeer = previous.lastPeer;
	out.peerSeenAt = previous.peerSeenAt;
	out.presentSeen = previous.presentSeen;

	const uintptr_t sessionAddress = RvaToAddress(GameOffsets::kGgpoSession);

	out.resolved = sessionAddress != 0;

	if (out.resolved)
		out.session = ReadDword(sessionAddress);

	if (out.session >= 0x10000)
		out.backend = BackendOf(ReadDword(out.session));

	if (out.backend == NetLink::Backend_Players)
		ReadPlayers(out.session, out);

	if (out.backend == NetLink::Backend_Spectator)
		ReadSpectator(out.session, out);

	if (out.hasPeer)
	{
		out.lastPeer = out.peer.id;
		out.peerSeenAt = out.tick;
	}

	out.lobby = ReadLobby();
	out.netplayActive = ReadByte(RvaToAddress(GameOffsets::kNetplayActive)) != 0;
	out.netplayFrame = static_cast<int>(ReadDword(RvaToAddress(GameOffsets::kNetplayFrame)));
	out.rollbacks = static_cast<int>(ReadDword(RvaToAddress(GameOffsets::kRollbackCount)));

	return out;
}

void LogSession(const NetLink::Snapshot& before, const NetLink::Snapshot& after)
{
	if (before.session == after.session && before.backend == after.backend)
		return;

	NetLog::Write("ggpo session 0x%08x -> 0x%08x, %s, players %d, spectators %d, layout %s",
		before.session, after.session, NetLink::BackendName(after.backend), after.players, after.spectators,
		after.layoutValid ? "ok" : "not validated");
}

void LogSync(const NetLink::Snapshot& before, const NetLink::Snapshot& after)
{
	if (before.synchronizing == after.synchronizing)
		return;

	if (after.synchronizing)
	{
		g_syncStart = after.tick;
		NetLog::Write("ggpo synchronizing started");
		return;
	}

	NetLog::Write("ggpo synchronizing ended after %lu ms", g_syncStart != 0 ? after.tick - g_syncStart : 0ul);
	g_syncStart = 0;
}

void LogPeer(const NetLink::Snapshot& before, const NetLink::Snapshot& after)
{
	if (after.hasPeer && (!before.hasPeer || before.peer.id != after.peer.id))
	{
		NetLog::Write("peer %llu connected, endpoint %s, ping %d ms", static_cast<unsigned long long>(after.peer.id),
			NetLink::StateName(after.peer.state), after.peer.ping);
		return;
	}

	if (before.hasPeer && !after.hasPeer)
	{
		NetLog::Write("peer %llu gone, last endpoint %s, pending %d, ping %d ms",
			static_cast<unsigned long long>(before.peer.id), NetLink::StateName(before.peer.state),
			before.peer.pending, before.peer.ping);
		return;
	}

	if (after.hasPeer && before.peer.state != after.peer.state)
	{
		NetLog::Write("peer %llu endpoint %s -> %s, pending %d, ping %d ms",
			static_cast<unsigned long long>(after.peer.id), NetLink::StateName(before.peer.state),
			NetLink::StateName(after.peer.state), after.peer.pending, after.peer.ping);
	}
}

void LogRest(const NetLink::Snapshot& before, const NetLink::Snapshot& after)
{
	if (before.spectators != after.spectators && after.backend == NetLink::Backend_Players)
		NetLog::Write("ggpo spectators %d -> %d", before.spectators, after.spectators);

	if (before.lobby != after.lobby)
		NetLog::Write("lobby %llu -> %llu", static_cast<unsigned long long>(before.lobby),
			static_cast<unsigned long long>(after.lobby));

	if (before.netplayActive != after.netplayActive)
		NetLog::Write("netplay %s at frame %d with %d rollback(s)", after.netplayActive ? "started" : "ended",
			after.netplayFrame, after.rollbacks);

	if (after.backend != NetLink::Backend_Players || after.layoutValid || g_blindLogged)
		return;

	g_blindLogged = true;
	NetLog::Write("ggpo layout did not validate: players %d, spectators %d. Online is assumed while this session lives",
		after.players, after.spectators);
}

constexpr int kPingMax = 60000;
constexpr int kKbpsMax = 100000;
constexpr int kBehindMax = 3600;

const char* Plausible(int value, int low, int high, char* out, int size)
{
	if (value < low || value > high)
		strncpy_s(out, size, "-", _TRUNCATE);
	else
		_snprintf_s(out, size, _TRUNCATE, "%d", value);

	return out;
}

double Milliseconds(int64_t ticks)
{
	return g_frequency.QuadPart != 0 ? static_cast<double>(ticks) * 1000.0 / static_cast<double>(g_frequency.QuadPart) : 0.0;
}

void ResetWindow(const NetLink::Snapshot& snapshot)
{
	g_window = {};
	g_window.start = snapshot.tick;
	g_window.startFrame = snapshot.netplayFrame;
	g_window.startRollbacks = snapshot.rollbacks;
}

void Summarise(const NetLink::Snapshot& snapshot)
{
	if (snapshot.tick - g_window.start < kSummaryMs)
	{
		if (snapshot.hasPeer)
			g_window.maxPending = (std::max)(g_window.maxPending, snapshot.peer.pending);

		return;
	}

	if (NetLink::InSession(snapshot) && g_window.frames > 0)
	{
		const NetLink::Endpoint& peer = snapshot.peer;

		const int frames = snapshot.netplayFrame - g_window.startFrame;
		const int rollbacks = snapshot.rollbacks - g_window.startRollbacks;

		char netplay[48] = {};

		if (frames < 0 || rollbacks < 0)
			strncpy_s(netplay, "netplay counters reset", _TRUNCATE);
		else
			_snprintf_s(netplay, _TRUNCATE, "netplay +%d rb +%d", frames, rollbacks);

		char ping[16] = {};
		char kbps[16] = {};
		char local[16] = {};
		char remote[16] = {};

		NetLog::Write("sec frames %d, interval avg %.1f max %.1f ms, %d over 20 | mod avg %.2f max %.2f ms | "
			"%s | %s ping %s kbps %s behind %s/%s pend %d max %d%s",
			g_window.frames, Milliseconds(g_window.intervalSum) / g_window.frames, Milliseconds(g_window.intervalMax),
			g_window.slow, Milliseconds(g_window.modSum) / g_window.frames, Milliseconds(g_window.modMax),
			netplay, snapshot.hasPeer ? NetLink::StateName(peer.state) : "no peer",
			Plausible(peer.ping, 0, kPingMax, ping, sizeof(ping)),
			Plausible(peer.kbps, 0, kKbpsMax, kbps, sizeof(kbps)),
			Plausible(peer.localBehind, -kBehindMax, kBehindMax, local, sizeof(local)),
			Plausible(peer.remoteBehind, -kBehindMax, kBehindMax, remote, sizeof(remote)),
			peer.pending, g_window.maxPending, snapshot.synchronizing ? " syncing" : "");
	}
	else if (g_window.frames > 0 && (g_window.slow > 0 || Milliseconds(g_window.modMax) * 1000.0 > kHeavyModMicros))
	{
		NetLog::Write("perf frames %d, interval avg %.1f max %.1f ms, %d over 20 | mod avg %.2f max %.2f ms",
			g_window.frames, Milliseconds(g_window.intervalSum) / g_window.frames, Milliseconds(g_window.intervalMax),
			g_window.slow, Milliseconds(g_window.modSum) / g_window.frames, Milliseconds(g_window.modMax));
	}

	GgpoLogCapture::ReportSecond();
	ResetWindow(snapshot);
}

}

void NetLink::OnPresent(int64_t modMicros)
{
	LARGE_INTEGER now = {};
	QueryPerformanceCounter(&now);

	if (g_frequency.QuadPart == 0)
		QueryPerformanceFrequency(&g_frequency);

	g_now.presentSeen = true;

	const int64_t interval = g_lastPresent.QuadPart != 0 ? now.QuadPart - g_lastPresent.QuadPart : 0;
	g_lastPresent = now;

	if (interval <= 0)
		return;

	const int64_t modTicks = modMicros * g_frequency.QuadPart / 1000000;

	++g_window.frames;
	g_window.intervalSum += interval;
	g_window.intervalMax = (std::max)(g_window.intervalMax, interval);
	g_window.modSum += modTicks;
	g_window.modMax = (std::max)(g_window.modMax, modTicks);

	const int64_t intervalMicros = interval * 1000000 / g_frequency.QuadPart;

	if (intervalMicros > kSlowMicros)
		++g_window.slow;

	if (intervalMicros <= kHitchMicros || !InSession(g_now) || g_window.hitches >= kHitchesPerSecond)
		return;

	++g_window.hitches;
	NetLog::Write("hitch: a frame took %.1f ms, mod work %.2f ms, netplay frame %d", intervalMicros / 1000.0,
		modMicros / 1000.0, g_now.netplayFrame);
}

void NetLink::NoteFocusChange(bool focused)
{
	if (!InSession(g_now))
		return;

	NetLog::Write("focus %s, netplay frame %d", focused ? "gained" : "lost", g_now.netplayFrame);
}

void NetLink::Update()
{
	const Snapshot before = g_now;
	Snapshot after = Read(before);

	LogSession(before, after);
	LogSync(before, after);
	LogPeer(before, after);
	LogRest(before, after);

	g_now = after;
	Summarise(after);

	AcquireSRWLockExclusive(&g_lock);
	g_shared = after;
	ReleaseSRWLockExclusive(&g_lock);
}

const NetLink::Snapshot& NetLink::Current()
{
	return g_now;
}

void NetLink::Copy(Snapshot& out)
{
	AcquireSRWLockShared(&g_lock);
	out = g_shared;
	ReleaseSRWLockShared(&g_lock);
}

bool NetLink::InSession(const Snapshot& snapshot)
{
	if (snapshot.backend == Backend_Spectator)
		return true;

	if (snapshot.backend == Backend_Players && (snapshot.hasPeer || !snapshot.layoutValid))
		return true;

	return snapshot.lastPeer != 0 && GetTickCount() - snapshot.peerSeenAt < kSessionHoldMs;
}

bool NetLink::InSession()
{
	return InSession(g_now);
}

bool NetLink::HasPeer()
{
	return g_now.hasPeer;
}

uint64_t NetLink::Peer()
{
	return g_now.hasPeer ? g_now.peer.id : g_now.lastPeer;
}

bool NetLink::PeerSeenWithin(DWORD milliseconds)
{
	return g_now.lastPeer != 0 && GetTickCount() - g_now.peerSeenAt < milliseconds;
}

bool NetLink::IsBlind()
{
	return !g_now.resolved || (g_now.backend == Backend_Players && !g_now.layoutValid);
}

uint64_t NetLink::Lobby()
{
	return g_now.lobby;
}

const char* NetLink::BackendName(Backend backend)
{
	switch (backend)
	{
	case Backend_Players:
		return "players";
	case Backend_Spectator:
		return "spectator";
	case Backend_Unknown:
		return "unknown backend";
	default:
		return "none";
	}
}

const char* NetLink::StateName(uint32_t state)
{
	switch (state)
	{
	case 0:
		return "syncing";
	case 1:
		return "synchronized";
	case 2:
		return "running";
	case 3:
		return "disconnected";
	default:
		return "?";
	}
}
