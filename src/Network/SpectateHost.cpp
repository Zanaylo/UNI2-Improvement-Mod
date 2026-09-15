#include "Network/SpectateHost.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"
#include "Network/GgpoSpectators.h"
#include "Network/SpectateCode.h"
#include "Network/SpectateMatch.h"
#include "Network/SpectateWire.h"
#include "Network/SpectatorPacing.h"
#include "Network/SteamFriends.h"
#include "Network/SteamInterfaces.h"
#include "Network/SteamNetwork.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

typedef void(__fastcall* StartSessionFn)(void*);
typedef void(__fastcall* DispatchMatchFn)(int*);

using Viewer = SpectateHost::Viewer;

constexpr DWORD kArmedFreshMs = 3000;
constexpr DWORD kPresenceEveryMs = 30000;
constexpr DWORD kStageResolveMs = 4000;
constexpr uint32_t kViewerSyncTimeoutMs = 5000;
constexpr int kFewestViewers = 1;
constexpr int kMostPendingFrames = 90;

StartSessionFn oStartSession = nullptr;
DispatchMatchFn oDispatchMatch = nullptr;

SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<Viewer> g_viewers;

bool g_allowed = false;
int g_maxViewers = SpectateHost::kMostViewers;
bool g_presenceShown = false;
DWORD g_presenceAt = 0;
int g_slowest = 0;
bool g_hadSession = false;

SpectateWire::MatchStart g_start = {};
std::vector<uint64_t> g_startTargets;
bool g_startWaiting = false;
bool g_startLoading = false;
DWORD g_startAt = 0;

char g_code[SpectateCode::kTextBytes] = "";
char g_status[192] = "off";

void Send(uint64_t to, SpectateWire::Type type, uint8_t detail)
{
	const SpectateWire::Message message = SpectateWire::Make(type, detail);
	SteamNetwork::SendTo(to, &message, sizeof(message));
}

uint32_t ReadGame(uintptr_t rva)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value);

	return value;
}

int IndexOf(uint64_t id)
{
	for (size_t i = 0; i < g_viewers.size(); ++i)
	{
		if (g_viewers[i].id == id)
			return static_cast<int>(i);
	}

	return -1;
}

int AcceptedCount()
{
	return static_cast<int>(std::count_if(g_viewers.begin(), g_viewers.end(),
		[](const Viewer& viewer) { return viewer.state == SpectateHost::Viewer_Accepted; }));
}

template <typename Fn>
void WithViewer(uint64_t id, Fn change)
{
	AcquireSRWLockExclusive(&g_lock);

	const int index = IndexOf(id);

	if (index >= 0)
		change(g_viewers[index]);

	ReleaseSRWLockExclusive(&g_lock);
}

SpectateWire::Type Admit(uint64_t id, uint8_t& detail)
{
	detail = SpectateWire::Refusal_None;

	if (!g_allowed)
	{
		detail = SpectateWire::Refusal_Closed;
		return SpectateWire::Type_Refused;
	}

	const int index = IndexOf(id);

	if (index >= 0 && g_viewers[index].state == SpectateHost::Viewer_Kicked)
		g_viewers[index].state = SpectateHost::Viewer_Pending;

	if (index >= 0)
		return g_viewers[index].state == SpectateHost::Viewer_Accepted ? SpectateWire::Type_Accepted : SpectateWire::Type_Pending;

	if (AcceptedCount() >= g_maxViewers)
	{
		detail = SpectateWire::Refusal_Full;
		return SpectateWire::Type_Refused;
	}

	Viewer viewer = {};
	viewer.id = id;
	viewer.state = SpectateHost::Viewer_Accepted;
	g_viewers.push_back(viewer);

	return SpectateWire::Type_Accepted;
}

void OnJoin(uint64_t from)
{
	uint8_t detail = SpectateWire::Refusal_None;

	AcquireSRWLockExclusive(&g_lock);
	const SpectateWire::Type answer = Admit(from, detail);
	ReleaseSRWLockExclusive(&g_lock);

	Send(from, answer, detail);
	LOG("SpectateHost: %llu asked to watch, answered %d (%d)", static_cast<unsigned long long>(from), answer, detail);
}

void OnArmed(uint64_t from)
{
	WithViewer(from, [](Viewer& viewer)
	{
		if (viewer.state == SpectateHost::Viewer_Accepted)
			viewer.armedAt = GetTickCount();
	});
}

void OnLeave(uint64_t from)
{
	AcquireSRWLockExclusive(&g_lock);

	g_viewers.erase(std::remove_if(g_viewers.begin(), g_viewers.end(),
		[from](const Viewer& viewer) { return viewer.id == from && viewer.state != SpectateHost::Viewer_Kicked; }),
		g_viewers.end());

	ReleaseSRWLockExclusive(&g_lock);
}

std::vector<uint64_t> ArmedViewers(DWORD now, bool preparedOnly)
{
	std::vector<uint64_t> armed;

	AcquireSRWLockShared(&g_lock);

	for (const Viewer& viewer : g_viewers)
	{
		const bool fresh = viewer.armedAt != 0 && now - viewer.armedAt < kArmedFreshMs;

		if (viewer.state == SpectateHost::Viewer_Accepted && fresh && (!preparedOnly || viewer.prepared))
			armed.push_back(viewer.id);
	}

	ReleaseSRWLockShared(&g_lock);

	return armed;
}

std::vector<uint64_t> WatchingViewers()
{
	std::vector<uint64_t> watching;

	AcquireSRWLockShared(&g_lock);

	for (const Viewer& viewer : g_viewers)
	{
		if (viewer.watching)
			watching.push_back(viewer.id);
	}

	ReleaseSRWLockShared(&g_lock);

	return watching;
}

void ClearWatching()
{
	AcquireSRWLockExclusive(&g_lock);

	for (Viewer& viewer : g_viewers)
		viewer.watching = false;

	ReleaseSRWLockExclusive(&g_lock);
}

bool LocalPlays(int command)
{
	if (command != GameOffsets::kMatchCaseRank && command != GameOffsets::kMatchCasePlayer)
		return false;

	return static_cast<int32_t>(ReadGame(GameOffsets::kMatchLocalSide)) >= 0;
}

void PrepareViewers()
{
	std::vector<uint64_t> armed = ArmedViewers(GetTickCount(), false);

	if (armed.empty())
		return;

	g_start = {};
	g_start.message = SpectateWire::Make(SpectateWire::Type_MatchStart, 0);

	if (!SpectateMatch::Capture(g_start.snapshot))
	{
		LOG("SpectateHost: the match could not be read, so no viewer joins it");
		return;
	}

	g_startTargets.swap(armed);
	g_startWaiting = true;
	g_startLoading = false;
	g_startAt = GetTickCount();
}

void SendStart()
{
	g_startWaiting = false;
	SpectateMatch::ResolveStage(g_start.snapshot);

	for (uint64_t id : g_startTargets)
	{
		const bool sent = SteamNetwork::SendTo(id, &g_start, sizeof(g_start));
		WithViewer(id, [sent](Viewer& viewer) { viewer.prepared = sent; });

		LOG("SpectateHost: match setup %s %llu, stage %d '%s', data %s", sent ? "sent to" : "could not reach",
			static_cast<unsigned long long>(id), SpectateMatch::StageOf(g_start.snapshot),
			SpectateMatch::StageNameOf(g_start.snapshot), SpectateMatch::PatchOf(g_start.snapshot));
	}

	g_startTargets.clear();
}

void WaitForStage(DWORD now)
{
	if (!g_startWaiting)
		return;

	if (ReadGame(GameOffsets::kBattleStartState) < static_cast<uint32_t>(GameOffsets::kBattleStartVsScreen))
	{
		g_startLoading = true;
		return;
	}

	if (g_startLoading || now - g_startAt >= kStageResolveMs)
		SendStart();
}

void AddViewers()
{
	if (!g_allowed)
		return;

	const uintptr_t session = GgpoSpectators::PeerSession();

	if (session == 0 || !GgpoSpectators::Synchronizing(session))
		return;

	for (uint64_t id : ArmedViewers(GetTickCount(), true))
	{
		const int result = GgpoSpectators::Add(session, id, kViewerSyncTimeoutMs);

		if (result == GgpoSpectators::kAddOk)
			SpectatorPacing::Pace(session, GgpoSpectators::IndexOf(session, id));

		WithViewer(id, [result](Viewer& viewer)
		{
			viewer.watching = result == GgpoSpectators::kAddOk;
			viewer.prepared = false;
		});

		LOG("SpectateHost: %llu %s this match (%d)", static_cast<unsigned long long>(id),
			result == GgpoSpectators::kAddOk ? "joins" : "could not join", result);

		if (result == GgpoSpectators::kAddFull || result == GgpoSpectators::kAddTooLate)
			return;
	}
}

void __fastcall HookedStartSession(void* holder)
{
	SpectatorPacing::Reset();
	oStartSession(holder);
	AddViewers();
}

bool Watching(uint64_t id)
{
	bool watching = false;
	WithViewer(id, [&watching](Viewer& viewer) { watching = viewer.watching; });

	return watching;
}

void DropBehind(uintptr_t session, uint64_t id, int pending)
{
	WithViewer(id, [](Viewer& viewer) { viewer.watching = false; });
	GgpoSpectators::Drop(session, id);
	Send(id, SpectateWire::Type_Behind, SpectateWire::Refusal_None);

	LOG("SpectateHost: %llu fell %d frames behind and was dropped from this match", static_cast<unsigned long long>(id), pending);
}

void WatchLag(uintptr_t session)
{
	GgpoSpectators::Link links[GameOffsets::kGgpoMostSpectators] = {};
	const int count = GgpoSpectators::Links(session, links, GameOffsets::kGgpoMostSpectators);

	g_slowest = 0;

	for (int i = 0; i < count; ++i)
	{
		const GgpoSpectators::Link& link = links[i];

		if (link.state != GameOffsets::kGgpoStateRunning || !Watching(link.id))
			continue;

		if (link.pending >= kMostPendingFrames)
		{
			DropBehind(session, link.id, link.pending);
			continue;
		}

		g_slowest = (std::max)(g_slowest, link.pending);
	}
}

void EndMatch()
{
	for (uint64_t id : WatchingViewers())
	{
		Send(id, SpectateWire::Type_MatchEnd, SpectateWire::Refusal_None);
		LOG("SpectateHost: told %llu the match ended", static_cast<unsigned long long>(id));
	}

	ClearWatching();
	SpectatorPacing::Reset();
	g_slowest = 0;
}

void __fastcall HookedDispatchMatch(int* command)
{
	const int kind = command != nullptr ? *command : 0;

	oDispatchMatch(command);

	if (g_allowed && LocalPlays(kind))
		PrepareViewers();
}

template <typename Fn>
bool Install(uintptr_t rva, Fn detour, Fn& original, const char* label)
{
	if (original != nullptr)
		return true;

	void* const target = reinterpret_cast<void*>(RvaToAddress(rva));

	if (IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)) &&
		HookManager::CreateAndEnableHook(target, detour, reinterpret_cast<void**>(&original), label))
	{
		return true;
	}

	original = nullptr;
	LOG("SpectateHost: %s is not where this game version expects it", label);
	return false;
}

void RefreshCode()
{
	if (g_code[0] != 0)
		return;

	const uint64_t own = SteamInterfaces::GetOwnSteamId();

	if (own != 0)
		SpectateCode::Encode(own, g_code, sizeof(g_code));
}

void Publish(DWORD now)
{
	RefreshCode();

	if (!g_allowed)
	{
		if (g_presenceShown && SteamFriends::SetRichPresence(SpectateWire::kPresenceKey, ""))
			g_presenceShown = false;

		return;
	}

	if (g_code[0] == 0 || (g_presenceShown && now - g_presenceAt < kPresenceEveryMs))
		return;

	if (!SteamFriends::SetRichPresence(SpectateWire::kPresenceKey, g_code))
		return;

	g_presenceShown = true;
	g_presenceAt = now;
}

void Summarise()
{
	AcquireSRWLockShared(&g_lock);

	const int accepted = AcceptedCount();
	const int watching = static_cast<int>(std::count_if(g_viewers.begin(), g_viewers.end(),
		[](const Viewer& viewer) { return viewer.watching; }));

	ReleaseSRWLockShared(&g_lock);

	if (!g_allowed)
	{
		strncpy_s(g_status, "off", _TRUNCATE);
		return;
	}

	sprintf_s(g_status, "on, %d of %d viewer(s) accepted, %d watching this match. Slowest viewer %d frames behind",
		accepted, g_maxViewers, watching, g_slowest);
}

}

void SpectateHost::Initialize()
{
	g_allowed = g_settings.spectateAllow != 0;
	g_maxViewers = std::clamp(g_settings.spectateMaxViewers, kFewestViewers, kMostViewers);

	Install(GameOffsets::kFnStartSessionFromDescription, &HookedStartSession, oStartSession, "StartSessionFromDescription");
	Install(GameOffsets::kFnMatchDispatch, &HookedDispatchMatch, oDispatchMatch, "MatchDispatch");
	SpectatorPacing::Install();
}

void SpectateHost::Update()
{
	const DWORD now = GetTickCount();

	Publish(now);
	WaitForStage(now);

	const uintptr_t session = GgpoSpectators::PeerSession();

	if (session != 0)
	{
		g_hadSession = true;
		WatchLag(session);
		Summarise();
		return;
	}

	if (g_hadSession)
		EndMatch();

	g_hadSession = false;
	Summarise();
}

void SpectateHost::Receive(uint8_t type, uint64_t from)
{
	switch (type)
	{
	case SpectateWire::Type_Join:
		OnJoin(from);
		return;
	case SpectateWire::Type_Armed:
		OnArmed(from);
		return;
	case SpectateWire::Type_Leave:
		OnLeave(from);
		return;
	default:
		return;
	}
}

bool SpectateHost::IsAllowed()
{
	return g_allowed && oStartSession != nullptr && oDispatchMatch != nullptr;
}

void SpectateHost::SetAllowed(bool allowed)
{
	g_allowed = allowed;
	Settings::SaveInt("Spectate", "AllowSpectators", allowed ? 1 : 0);
	LOG("SpectateHost: spectators are %s", allowed ? "allowed" : "not allowed");
}

int SpectateHost::MaxViewers()
{
	return g_maxViewers;
}

void SpectateHost::SetMaxViewers(int count)
{
	g_maxViewers = std::clamp(count, kFewestViewers, kMostViewers);
	Settings::SaveInt("Spectate", "MaxViewers", g_maxViewers);
}

const char* SpectateHost::Code()
{
	RefreshCode();
	return g_code;
}

void SpectateHost::Snapshot(std::vector<Viewer>& out)
{
	AcquireSRWLockShared(&g_lock);
	out = g_viewers;
	ReleaseSRWLockShared(&g_lock);
}

void SpectateHost::Approve(uint64_t id)
{
	AcquireSRWLockExclusive(&g_lock);

	const int index = IndexOf(id);
	const bool approved = index >= 0 && g_viewers[index].state != Viewer_Accepted && AcceptedCount() < g_maxViewers;

	if (approved)
		g_viewers[index].state = Viewer_Accepted;

	ReleaseSRWLockExclusive(&g_lock);

	if (approved)
		Send(id, SpectateWire::Type_Accepted, SpectateWire::Refusal_None);
}

void SpectateHost::Kick(uint64_t id)
{
	AcquireSRWLockExclusive(&g_lock);

	const int index = IndexOf(id);

	if (index >= 0)
	{
		g_viewers[index].state = Viewer_Kicked;
		g_viewers[index].prepared = false;
		g_viewers[index].watching = false;
	}

	ReleaseSRWLockExclusive(&g_lock);

	if (index < 0)
		return;

	GgpoSpectators::Drop(GgpoSpectators::PeerSession(), id);
	Send(id, SpectateWire::Type_Kicked, SpectateWire::Refusal_None);
	LOG("SpectateHost: %llu was removed", static_cast<unsigned long long>(id));
}

void SpectateHost::Forget(uint64_t id)
{
	AcquireSRWLockExclusive(&g_lock);

	g_viewers.erase(std::remove_if(g_viewers.begin(), g_viewers.end(),
		[id](const Viewer& viewer) { return viewer.id == id; }), g_viewers.end());

	ReleaseSRWLockExclusive(&g_lock);

	Send(id, SpectateWire::Type_Refused, SpectateWire::Refusal_Declined);
}

const char* SpectateHost::StatusText()
{
	return g_status;
}
