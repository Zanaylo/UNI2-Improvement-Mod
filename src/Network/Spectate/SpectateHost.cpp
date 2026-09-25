#include "Network/Spectate/SpectateHost.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/GameHook.h"
#include "Hooks/HookManager.h"
#include "Network/ModChannel.h"
#include "Network/NetLog.h"
#include "Network/Spectate/SpectateCode.h"
#include "Network/Spectate/SpectateMatch.h"
#include "Network/Spectate/SpectateRelay.h"
#include "Network/Spectate/SpectateWire.h"
#include "Network/Steam/SteamFriends.h"
#include "Network/Steam/SteamInterfaces.h"
#include "Game/Engine/CodeSignatures.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

typedef void(__fastcall* DispatchMatchFn)(int*);

using Viewer = SpectateHost::Viewer;

constexpr DWORD kArmedFreshMs = 3000;
constexpr DWORD kPresenceEveryMs = 30000;
constexpr DWORD kStageResolveMs = 4000;
constexpr int kFewestViewers = 1;
constexpr DWORD kMessageTtlMs = 5000;
constexpr DWORD kStartTtlMs = 10000;
constexpr DWORD kInputsTtlMs = 3000;
constexpr DWORD kStreamEveryMs = 100;
constexpr DWORD kRewindEveryMs = 2000;
constexpr int kResendGapFrames = 240;
constexpr int kMostBehindFrames = 600;
constexpr int kMostQueued = 8;

GameHook<DispatchMatchFn> g_dispatchMatchHook("MatchDispatch");

SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<Viewer> g_viewers;

bool g_allowed = false;
bool g_hooksActive = false;
volatile LONG g_codeReady = 0;
int g_maxViewers = SpectateHost::kMostViewers;
bool g_presenceShown = false;
DWORD g_presenceAt = 0;
int g_slowest = 0;
bool g_hadSession = false;
DWORD g_streamedAt = 0;

SpectateWire::MatchStart g_start = {};
std::vector<uint64_t> g_startTargets;
bool g_startWaiting = false;
bool g_startLoading = false;
DWORD g_startAt = 0;

uint8_t g_batch[sizeof(SpectateWire::Inputs) + SpectateWire::kMostFramesPerBatch * SpectateWire::kInputBytes] = {};

char g_code[SpectateCode::kTextBytes] = "";
char g_status[192] = "off";

void Send(uint64_t to, SpectateWire::Type type, uint8_t detail)
{
	const SpectateWire::Message message = SpectateWire::Make(type, detail);
	ModChannel::SendTo(to, &message, sizeof(message), kMessageTtlMs, "spectate");
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
	viewer.sentThrough = -1;
	viewer.ackedFrame = -1;
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

void OnAck(uint64_t from, const uint8_t* data, int size)
{
	if (size < static_cast<int>(sizeof(SpectateWire::Ack)))
		return;

	SpectateWire::Ack ack = {};
	memcpy(&ack, data, sizeof(ack));

	WithViewer(from, [&ack](Viewer& viewer)
	{
		if (!viewer.watching || ack.frame < viewer.ackedFrame)
			return;

		viewer.ackedFrame = ack.frame;
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

std::vector<uint64_t> ArmedViewers(DWORD now)
{
	std::vector<uint64_t> armed;

	AcquireSRWLockShared(&g_lock);

	for (const Viewer& viewer : g_viewers)
	{
		const bool fresh = viewer.armedAt != 0 && now - viewer.armedAt < kArmedFreshMs;

		if (viewer.state == SpectateHost::Viewer_Accepted && fresh)
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

bool LocalPlays(int command)
{
	if (command != GameOffsets::kMatchCaseRank && command != GameOffsets::kMatchCasePlayer)
		return false;

	return static_cast<int32_t>(ReadGame(GameOffsets::kMatchLocalSide)) >= 0;
}

void PrepareViewers()
{
	std::vector<uint64_t> armed = ArmedViewers(GetTickCount());

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
		const bool queued = ModChannel::SendTo(id, &g_start, sizeof(g_start), kStartTtlMs, "spectate match");
		WithViewer(id, [queued](Viewer& viewer) { viewer.prepared = queued; });

		LOG("SpectateHost: match setup %s %llu, stage %d '%s', data %s", queued ? "queued for" : "could not queue for",
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

void BeginMatch(DWORD now)
{
	SpectateRelay::Reset();

	AcquireSRWLockExclusive(&g_lock);

	for (Viewer& viewer : g_viewers)
	{
		const bool fresh = viewer.armedAt != 0 && now - viewer.armedAt < kArmedFreshMs;

		viewer.watching = g_allowed && viewer.state == SpectateHost::Viewer_Accepted && viewer.prepared && fresh;
		viewer.prepared = false;
		viewer.sentThrough = -1;
		viewer.ackedFrame = -1;
		viewer.rewoundAt = now;

		if (viewer.watching)
			NetLog::Write("spectate host: %llu watches this match through the relay", static_cast<unsigned long long>(viewer.id));
	}

	ReleaseSRWLockExclusive(&g_lock);
}

int PackBatch(int first, int last)
{
	const int bytes = SpectateRelay::InputBytes();
	int count = 0;

	for (int frame = first; frame <= last && count < SpectateWire::kMostFramesPerBatch; ++frame, ++count)
	{
		if (!SpectateRelay::Read(frame, g_batch + sizeof(SpectateWire::Inputs) + count * bytes))
			break;
	}

	if (count == 0)
		return 0;

	SpectateWire::Inputs header = {};
	header.message = SpectateWire::Make(SpectateWire::Type_Inputs, 0);
	header.first = first;
	header.count = static_cast<uint16_t>(count);
	header.bytes = static_cast<uint16_t>(bytes);
	memcpy(g_batch, &header, sizeof(header));

	return count;
}

void Rewind(Viewer& viewer, DWORD now)
{
	if (viewer.sentThrough - viewer.ackedFrame < kResendGapFrames || now - viewer.rewoundAt < kRewindEveryMs)
		return;

	viewer.sentThrough = viewer.ackedFrame;
	viewer.rewoundAt = now;
}

void Stream(DWORD now)
{
	if (now - g_streamedAt < kStreamEveryMs || SpectateRelay::InputBytes() == 0)
		return;

	g_streamedAt = now;

	const int confirmed = SpectateRelay::Confirmed();

	AcquireSRWLockExclusive(&g_lock);

	for (Viewer& viewer : g_viewers)
	{
		if (!viewer.watching || ModChannel::Queued() >= kMostQueued)
			continue;

		Rewind(viewer, now);

		const int count = PackBatch(viewer.sentThrough + 1, confirmed);

		if (count == 0)
			continue;

		const int size = static_cast<int>(sizeof(SpectateWire::Inputs)) + count * SpectateRelay::InputBytes();

		if (ModChannel::SendTo(viewer.id, g_batch, size, kInputsTtlMs, "spectate inputs"))
			viewer.sentThrough += count;
	}

	ReleaseSRWLockExclusive(&g_lock);
}

void WatchLag()
{
	const int confirmed = SpectateRelay::Confirmed();
	std::vector<uint64_t> behind;

	AcquireSRWLockExclusive(&g_lock);

	g_slowest = 0;

	for (Viewer& viewer : g_viewers)
	{
		if (!viewer.watching)
			continue;

		const int lag = confirmed - viewer.ackedFrame;

		if (lag > kMostBehindFrames)
		{
			viewer.watching = false;
			behind.push_back(viewer.id);
			continue;
		}

		g_slowest = (std::max)(g_slowest, lag);
	}

	ReleaseSRWLockExclusive(&g_lock);

	for (uint64_t id : behind)
	{
		Send(id, SpectateWire::Type_Behind, SpectateWire::Refusal_None);
		NetLog::Write("spectate host: %llu fell behind and stops watching this match", static_cast<unsigned long long>(id));
	}
}

void EndMatch()
{
	for (uint64_t id : WatchingViewers())
	{
		Send(id, SpectateWire::Type_MatchEnd, SpectateWire::Refusal_None);
		LOG("SpectateHost: told %llu the match ended", static_cast<unsigned long long>(id));
	}

	AcquireSRWLockExclusive(&g_lock);

	for (Viewer& viewer : g_viewers)
		viewer.watching = false;

	ReleaseSRWLockExclusive(&g_lock);

	SpectateRelay::Reset();
	g_slowest = 0;
}

void __fastcall HookedDispatchMatch(int* command)
{
	const int kind = command != nullptr ? *command : 0;

	g_dispatchMatchHook.Original()(command);

	if (g_allowed && LocalPlays(kind))
		PrepareViewers();
}

bool Activate(bool active)
{
	if (active == g_hooksActive)
		return true;

	void* const target = reinterpret_cast<void*>(CodeSignatures::Address(GameOffsets::kFnMatchDispatch));

	bool ready = false;

	if (g_dispatchMatchHook.IsLive())
	{
		ready = HookManager::SetHookEnabled(target, active);
	}
	else if (!active)
	{
		ready = true;
	}
	else if (IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		ready = g_dispatchMatchHook.Install(target, &HookedDispatchMatch);
	}

	if (!ready)
	{
		LOG("SpectateHost: the match setup is not where this game version expects it");
	}

	g_hooksActive = active && ready;
	NetLog::Write("spectate host hook %s", g_hooksActive ? "active" : "parked");
	return ready;
}

void RefreshCode()
{
	if (g_codeReady != 0)
		return;

	const uint64_t own = SteamInterfaces::GetOwnSteamId();

	if (own == 0)
		return;

	SpectateCode::Encode(own, g_code, sizeof(g_code));
	InterlockedExchange(&g_codeReady, 1);
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

	if (g_codeReady == 0 || (g_presenceShown && now - g_presenceAt < kPresenceEveryMs))
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
	g_maxViewers = std::clamp(g_settings.spectateMaxViewers, kFewestViewers, kMostViewers);
	g_allowed = g_settings.spectateAllow != 0 && Activate(true);
}

void SpectateHost::Tick(const NetLink::Snapshot&)
{
	Publish(GetTickCount());
}

void SpectateHost::Update()
{
	const DWORD now = GetTickCount();

	if (!g_allowed && !g_hadSession)
	{
		Summarise();
		return;
	}

	WaitForStage(now);

	const NetLink::Snapshot& link = NetLink::Current();
	const bool session = link.backend == NetLink::Backend_Players && link.hasPeer;

	if (session && !g_hadSession)
		BeginMatch(now);

	if (!session && g_hadSession)
		EndMatch();

	g_hadSession = session;

	if (session)
	{
		SpectateRelay::Capture(link.session);
		Stream(now);
		WatchLag();
	}

	Summarise();
}

void SpectateHost::Receive(uint8_t type, const uint8_t* data, int size, uint64_t from)
{
	switch (type)
	{
	case SpectateWire::Type_Join:
		OnJoin(from);
		return;
	case SpectateWire::Type_Armed:
		OnArmed(from);
		return;
	case SpectateWire::Type_Ack:
		OnAck(from, data, size);
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
	return g_allowed && g_hooksActive;
}

void SpectateHost::SetAllowed(bool allowed)
{
	if (!allowed)
		EndMatch();

	g_allowed = Activate(allowed) && allowed;
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
	return g_codeReady != 0 ? g_code : "";
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
