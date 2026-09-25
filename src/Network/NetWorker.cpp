#include "Network/NetWorker.h"

#include "Core/ThreadRole.h"
#include "Network/MatchKind.h"
#include "Network/ModChannel.h"
#include "Network/ModPresence.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/PlayerCount.h"
#include "Network/RoomPing.h"
#include "Network/Spectate/SpectateHost.h"
#include "Network/Spectate/SpectateViewer.h"
#include "Network/Steam/SteamInterfaces.h"
#include "Network/Steam/SteamLink.h"
#include "Network/Steam/SteamNetwork.h"

#include <Windows.h>

namespace {

constexpr DWORD kTickMs = 50;
constexpr DWORD kSteamSettleMs = 2000;
constexpr DWORD kMeasureMs = 1000;
constexpr double kSlowJobMs = 8.0;
constexpr DWORD kSlowReportMs = 5000;

using Job = void(*)(const NetLink::Snapshot&);

HANDLE g_thread = nullptr;
HANDLE g_stop = nullptr;
volatile LONG g_running = 0;

LARGE_INTEGER g_frequency = {};
double g_slowestMs = 0.0;
DWORD g_slowReportedAt = 0;

DWORD g_presentSeenAt = 0;
DWORD g_measuredAt = 0;

void Measure(const NetLink::Snapshot& snapshot)
{
	const DWORD now = GetTickCount();

	if (now - g_measuredAt < kMeasureMs || !NetLink::InSession(snapshot))
		return;

	g_measuredAt = now;
	SteamLink::Measure(snapshot.hasPeer ? snapshot.peer.id : snapshot.lastPeer);
}

void Channel(const NetLink::Snapshot& snapshot)
{
	ModChannel::Receive();
	ModChannel::Flush(snapshot);
}

void Presence(const NetLink::Snapshot& snapshot)
{
	ModPresence::Tick(snapshot);
}

void Ping(const NetLink::Snapshot& snapshot)
{
	RoomPing::Tick(snapshot);
}

void Kind(const NetLink::Snapshot& snapshot)
{
	MatchKind::Tick(snapshot);
}

void Players(const NetLink::Snapshot&)
{
	PlayerCount::Update();
}

void Spectators(const NetLink::Snapshot& snapshot)
{
	SpectateHost::Tick(snapshot);
	SpectateViewer::Tick(snapshot);
}

struct NamedJob
{
	const char* name;
	Job job;
};

constexpr NamedJob kJobs[] = {
	{ "mod channel", &Channel },
	{ "link measure", &Measure },
	{ "room presence", &Presence },
	{ "room ping", &Ping },
	{ "match kind", &Kind },
	{ "player count", &Players },
	{ "spectate", &Spectators },
};

void Run(const NamedJob& named, const NetLink::Snapshot& snapshot)
{
	LARGE_INTEGER before = {};
	QueryPerformanceCounter(&before);

	named.job(snapshot);

	LARGE_INTEGER after = {};
	QueryPerformanceCounter(&after);

	const double ms = static_cast<double>(after.QuadPart - before.QuadPart) * 1000.0 /
		static_cast<double>(g_frequency.QuadPart);

	if (ms > g_slowestMs)
		g_slowestMs = ms;

	const DWORD now = GetTickCount();

	if (ms < kSlowJobMs || now - g_slowReportedAt < kSlowReportMs)
		return;

	g_slowReportedAt = now;
	NetLog::Write("worker: %s took %.1f ms of Steam time%s", named.name, ms,
		NetLink::InSession(snapshot) ? " during a match" : "");
}

bool SteamSettled(const NetLink::Snapshot& snapshot)
{
	if (!snapshot.presentSeen)
		return false;

	const DWORD now = GetTickCount();

	if (g_presentSeenAt == 0)
		g_presentSeenAt = now;

	if (now - g_presentSeenAt < kSteamSettleMs)
		return false;

	if (!SteamInterfaces::IsReady())
		SteamInterfaces::Initialize();

	if (!SteamNetwork::IsReady())
		SteamNetwork::Initialize();

	return SteamInterfaces::IsReady();
}

DWORD WINAPI Loop(LPVOID)
{
	ThreadRole::Mark(ThreadRole::Role_Worker);
	QueryPerformanceFrequency(&g_frequency);
	NetLog::Write("worker: started, the mod's Steam calls run here and never on the game thread");

	NetLink::Snapshot snapshot = {};

	while (WaitForSingleObject(g_stop, kTickMs) == WAIT_TIMEOUT)
	{
		NetLink::Copy(snapshot);

		if (!SteamSettled(snapshot))
			continue;

		for (const NamedJob& named : kJobs)
			Run(named, snapshot);
	}

	InterlockedExchange(&g_running, 0);
	return 0;
}

}

void NetWorker::Start()
{
	if (g_thread != nullptr)
		return;

	g_stop = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	g_thread = CreateThread(nullptr, 0, &Loop, nullptr, 0, nullptr);

	if (g_thread == nullptr)
		return;

	SetThreadPriority(g_thread, THREAD_PRIORITY_BELOW_NORMAL);
	InterlockedExchange(&g_running, 1);
}

void NetWorker::Stop()
{
	if (g_stop != nullptr)
		SetEvent(g_stop);
}

bool NetWorker::IsRunning()
{
	return g_running != 0;
}

double NetWorker::SlowestJobMs()
{
	return g_slowestMs;
}
