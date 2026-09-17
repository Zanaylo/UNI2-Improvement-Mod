#include "Network/SpectateViewer.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GamePatches.h"
#include "Hooks/HookManager.h"
#include "Network/ModHandshake.h"
#include "Network/ReadyFlags.h"
#include "Network/SpectateCode.h"
#include "Network/SpectateFeed.h"
#include "Network/SpectateMatch.h"
#include "Network/SpectateWire.h"
#include "Network/SteamInterfaces.h"
#include "Network/ModChannel.h"
#include "Network/NetLog.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

typedef void(__fastcall* ChooseSessionFn)(void*, void*, int, int);
typedef void(__fastcall* StartSpectatorFn)(void*);
typedef void(__fastcall* DispatchFn)(int*);
typedef void(__cdecl* TeardownFn)();

using State = SpectateViewer::State;

constexpr DWORD kAskEveryMs = 2000;
constexpr int kAsks = 5;
constexpr DWORD kArmedEveryMs = 1000;
constexpr DWORD kEnterTimeoutMs = 20000;
constexpr DWORD kFriendsEveryMs = 3000;
constexpr DWORD kFriendsWantedMs = 5000;
constexpr DWORD kMessageTtlMs = 5000;
constexpr DWORD kAckEveryMs = 500;
constexpr DWORD kAckTtlMs = 1000;
constexpr DWORD kReportEveryMs = 1000;
constexpr DWORD kEndGraceMs = 30000;
constexpr uint32_t kGgpoPath = 1;
constexpr uint32_t kByteMask = 0xff;
constexpr const char* kLoopback = "127.0.0.1";
constexpr const char* kBackToMenu = "the match ended. You join the host's next one";
constexpr const char* kFellBehind = "you fell too far behind. You join the host's next match";

ChooseSessionFn oChooseSession = nullptr;

State g_state = SpectateViewer::State_Idle;
uint64_t g_host = 0;
DWORD g_sentAt = 0;
DWORD g_enteredAt = 0;
DWORD g_reportedAt = 0;
DWORD g_ackedAt = 0;
DWORD g_endedAt = 0;
int g_asks = 0;
bool g_matchReady = false;
bool g_sessionSeen = false;
bool g_leftRematchWait = false;
bool g_dropped = false;
SpectateMatch::Snapshot g_match = {};
uint8_t g_description[GameOffsets::kSessionDescriptionSize] = {};

SRWLOCK g_friendsLock = SRWLOCK_INIT;
std::vector<SteamFriends::Friend> g_friends;
DWORD g_friendsAt = 0;
volatile LONG g_friendsWantedAt = 0;
bool g_hooksActive = false;

char g_status[192] = "not watching anyone";

bool Activate(bool active);

void Become(State state, const char* status)
{
	g_state = state;
	strncpy_s(g_status, status, _TRUNCATE);
	LOG("SpectateViewer: %s", g_status);
	NetLog::Write("spectate viewer: %s", g_status);

	if (state == SpectateViewer::State_Idle)
		Activate(false);
}

void Send(SpectateWire::Type type)
{
	const SpectateWire::Message message = SpectateWire::Make(type, SpectateWire::Refusal_None);
	ModChannel::SendTo(g_host, &message, sizeof(message), kMessageTtlMs, "spectate");
	g_sentAt = GetTickCount();
}

uint32_t ReadGame(uintptr_t rva)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value);

	return value;
}

void WriteByte(uintptr_t rva, uint8_t value)
{
	TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(rva)), &value, sizeof(value));
}

void Put(uintptr_t offset, uint32_t value)
{
	memcpy(g_description + offset, &value, sizeof(value));
}

void Describe()
{
	memset(g_description, 0, sizeof(g_description));

	Put(GameOffsets::kDescriptionGgpo, kGgpoPath);
	Put(GameOffsets::kDescriptionLocalIndex, GameOffsets::kDescriptionSpectatorIndex);
	Put(GameOffsets::kDescriptionLocalPort, GameOffsets::kGgpoLocalPort);
	Put(GameOffsets::kDescriptionFrameDelay, ReadGame(GameOffsets::kInputDelayOption) & kByteMask);
	Put(GameOffsets::kDescriptionPadSlot, ReadGame(GameOffsets::kNetplayPadSlot));
	Put(GameOffsets::kDescriptionRemotePort, GameOffsets::kGgpoLocalPort);
	Put(GameOffsets::kDescriptionRemoteSteamLow, 0);
	Put(GameOffsets::kDescriptionRemoteSteamHigh, 0);

	strncpy_s(reinterpret_cast<char*>(g_description + GameOffsets::kDescriptionRemoteIp),
		GameOffsets::kDescriptionRemoteIpBytes, kLoopback, _TRUNCATE);
}

bool CallStartSpectator(void* holder)
{
	__try
	{
		reinterpret_cast<StartSpectatorFn>(RvaToAddress(GameOffsets::kFnStartSpectatorSession))(holder);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

bool StartSpectating(void* holder)
{
	Describe();

	const uintptr_t description = reinterpret_cast<uintptr_t>(g_description);

	return TryWriteMemory(holder, &description, sizeof(description)) && CallStartSpectator(holder);
}

void __fastcall HookedChooseSession(void* holder, void* edx, int first, int second)
{
	if (g_state != SpectateViewer::State_Entering || g_host == 0)
	{
		oChooseSession(holder, edx, first, second);
		return;
	}

	g_sessionSeen = false;
	SpectateFeed::Reset();

	if (!StartSpectating(holder))
	{
		Become(SpectateViewer::State_Waiting, "the spectator session could not start, waiting for the next match");
		return;
	}

	Become(SpectateViewer::State_Watching, "watching the match");
}

void OnMatchStart(const uint8_t* data, int size)
{
	if (g_state != SpectateViewer::State_Waiting || size < static_cast<int>(sizeof(SpectateWire::MatchStart)))
		return;

	SpectateWire::MatchStart start = {};
	memcpy(&start, data, sizeof(start));

	g_match = start.snapshot;
	g_match.stageName[SpectateMatch::kStageNameBytes - 1] = 0;
	g_match.patch[SpectateMatch::kPatchBytes - 1] = 0;
	g_matchReady = true;
}

void RefreshFriends(DWORD now)
{
	if (g_friendsWantedAt == 0 || now - static_cast<DWORD>(g_friendsWantedAt) > kFriendsWantedMs)
		return;

	if (g_friendsAt != 0 && now - g_friendsAt < kFriendsEveryMs)
		return;

	g_friendsAt = now;

	std::vector<SteamFriends::Friend> found;
	SteamFriends::WithRichPresence(SpectateWire::kPresenceKey, found);

	AcquireSRWLockExclusive(&g_friendsLock);
	g_friends.swap(found);
	ReleaseSRWLockExclusive(&g_friendsLock);
}

void Ask(DWORD now)
{
	if (g_sentAt != 0 && now - g_sentAt < kAskEveryMs)
		return;

	if (g_asks >= kAsks)
	{
		Become(SpectateViewer::State_Idle, "the host did not answer. They need the mod and spectating turned on");
		return;
	}

	++g_asks;
	Send(SpectateWire::Type_Join);
}

bool FollowPatch()
{
	const char* const patch = SpectateMatch::PatchOf(g_match);
	const int index = ModHandshake::IndexOfData(patch);
	char status[192] = {};

	if (index == ModHandshake::kNoData)
	{
		sprintf_s(status, "the host plays on %s, which you do not have. Waiting for the next match", patch);
		Become(SpectateViewer::State_Waiting, status);
		return false;
	}

	if (index == GamePatches::BootIndex() || GamePatches::SwitchTables(index, "watching the host"))
		return true;

	sprintf_s(status, "%s could not be loaded here. Waiting for the next match", patch);
	Become(SpectateViewer::State_Waiting, status);
	return false;
}

void Enter()
{
	g_matchReady = false;

	if (ReadGame(GameOffsets::kGgpoSession) != 0 || NetLink::Lobby() != 0)
	{
		Become(SpectateViewer::State_Waiting, "you are in your own match or room, so this match was skipped");
		return;
	}

	if (!FollowPatch())
		return;

	if (!SpectateMatch::Apply(g_match))
	{
		Become(SpectateViewer::State_Waiting, "the match could not be set up, waiting for the next one");
		return;
	}

	g_enteredAt = GetTickCount();
	g_endedAt = 0;
	g_sessionSeen = false;
	g_leftRematchWait = false;
	g_dropped = false;

	Become(SpectateViewer::State_Entering, "joining the match");
}

void Wait(DWORD now)
{
	if (g_matchReady)
	{
		Enter();
		return;
	}

	if (g_sentAt == 0 || now - g_sentAt >= kArmedEveryMs)
		Send(SpectateWire::Type_Armed);
}

void Report(DWORD now)
{
	if (g_reportedAt != 0 && now - g_reportedAt < kReportEveryMs)
		return;

	g_reportedAt = now;

	char state[160] = {};
	ReadyFlags::Describe(state, sizeof(state));
	LOG("SpectateViewer: entering, %s", state);
}

void ReportWatching(DWORD now)
{
	if (g_reportedAt != 0 && now - g_reportedAt < kReportEveryMs)
		return;

	g_reportedAt = now;

	sprintf_s(g_status, "watching, %d frames behind", SpectateFeed::Buffered());
	LOG("SpectateViewer: watching, %d frames buffered, target %d, %d stall(s), after match state 0x%x",
		SpectateFeed::Buffered(), SpectateFeed::Target(), SpectateFeed::Stalls(), ReadGame(GameOffsets::kAfterMatchState));
}

bool EndSession()
{
	if (ReadGame(GameOffsets::kGgpoSession) == 0)
		return true;

	__try
	{
		reinterpret_cast<TeardownFn>(RvaToAddress(GameOffsets::kFnSessionTeardown))();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

bool GoToMenu()
{
	int command[2] = { GameOffsets::kMatchCaseMainMenu, 0 };

	__try
	{
		reinterpret_cast<DispatchFn>(RvaToAddress(GameOffsets::kFnMatchDispatch))(command);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

void Finish(const char* status)
{
	LOG("SpectateViewer: leaving the match, replay recording %u, replay save result %u",
		ReadGame(GameOffsets::kReplayRecording), ReadGame(GameOffsets::kReplaySaveResult));

	if (!EndSession())
		LOG("SpectateViewer: the spectator session could not be closed");

	WriteByte(GameOffsets::kOnlineMatchFlag, 0);
	WriteByte(GameOffsets::kExternalInputFlag, 0);

	if (!GoToMenu())
		LOG("SpectateViewer: the game did not take the way back to the menu");

	g_sessionSeen = false;
	g_endedAt = 0;
	g_dropped = false;

	Become(SpectateViewer::State_Waiting, status);
}

void SendAck(DWORD now)
{
	if (now - g_ackedAt < kAckEveryMs)
		return;

	g_ackedAt = now;

	SpectateWire::Ack ack = {};
	ack.message = SpectateWire::Make(SpectateWire::Type_Ack, SpectateWire::Refusal_None);
	ack.frame = SpectateFeed::Newest();
	ModChannel::SendTo(g_host, &ack, sizeof(ack), kAckTtlMs, "spectate ack");
}

void OnInputs(const uint8_t* data, int size)
{
	if (!SpectateViewer::IsJoining() || size < static_cast<int>(sizeof(SpectateWire::Inputs)))
		return;

	SpectateWire::Inputs header = {};
	memcpy(&header, data, sizeof(header));

	const int expected = static_cast<int>(sizeof(header)) + header.count * header.bytes;

	if (header.bytes == 0 || header.bytes > SpectateWire::kInputBytes || size < expected)
		return;

	SpectateFeed::Store(header.first, header.count, header.bytes, data + sizeof(header));
}

void Arrive(DWORD now)
{
	ReadyFlags::HoldBattleStart();
	Report(now);
	SendAck(now);

	if (g_dropped)
	{
		Finish(kFellBehind);
		return;
	}

	if (g_sentAt == 0 || now - g_sentAt >= kArmedEveryMs)
		Send(SpectateWire::Type_Armed);

	if (now - g_enteredAt < kEnterTimeoutMs)
		return;

	Finish("the match never started here. You join the host's next one");
}

void Follow(DWORD now)
{
	ReadyFlags::HoldBattleStart();
	SpectateFeed::Pump();
	SendAck(now);

	if (g_dropped)
	{
		Finish(kFellBehind);
		return;
	}

	const bool session = ReadGame(GameOffsets::kGgpoSession) != 0;

	if (session)
	{
		g_sessionSeen = true;
		ReportWatching(now);
	}

	if (!g_sessionSeen)
		return;

	const bool awaitingRematch = ReadGame(GameOffsets::kAfterMatchState) == static_cast<uint32_t>(GameOffsets::kAfterMatchAwaitRematch);

	if (!awaitingRematch)
		g_leftRematchWait = true;

	if (awaitingRematch && g_leftRematchWait)
	{
		Finish(kBackToMenu);
		return;
	}

	if (!session && g_endedAt == 0)
		g_endedAt = now;

	if (g_endedAt != 0 && now - g_endedAt >= kEndGraceMs)
		Finish(kBackToMenu);
}

const char* RefusalText(uint8_t detail)
{
	switch (detail)
	{
	case SpectateWire::Refusal_Closed:
		return "the host is not allowing spectators";
	case SpectateWire::Refusal_Full:
		return "the host has no room for more viewers";
	default:
		return "the host declined";
	}
}

}

namespace {

bool HookSessionBuilder(bool active)
{
	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnChooseRoomSession));

	if (oChooseSession != nullptr)
		return HookManager::SetHookEnabled(target, active);

	if (!active)
		return true;

	if (IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)) &&
		HookManager::CreateAndEnableHook(target, &HookedChooseSession, reinterpret_cast<void**>(&oChooseSession),
			"ChooseRoomSession"))
	{
		return true;
	}

	oChooseSession = nullptr;
	LOG("SpectateViewer: the game's session builder is not where this game version expects it");
	return false;
}

bool Activate(bool active)
{
	if (active == g_hooksActive)
		return true;

	const bool builder = HookSessionBuilder(active);
	const bool feed = SpectateFeed::SetActive(active);

	g_hooksActive = active && builder && feed;
	NetLog::Write("spectate viewer hooks %s", g_hooksActive ? "active" : "parked");

	return builder && feed;
}

}

void SpectateViewer::Initialize()
{
}

void SpectateViewer::Tick(const NetLink::Snapshot&)
{
	RefreshFriends(GetTickCount());
}

void SpectateViewer::Update()
{
	const DWORD now = GetTickCount();

	switch (g_state)
	{
	case State_Asking:
		Ask(now);
		return;
	case State_Waiting:
		Wait(now);
		return;
	case State_Entering:
		Arrive(now);
		return;
	case State_Watching:
		Follow(now);
		return;
	default:
		return;
	}
}

void SpectateViewer::Receive(uint8_t type, uint8_t detail, const uint8_t* data, int size, uint64_t from)
{
	if (from == 0 || from != g_host)
		return;

	switch (type)
	{
	case SpectateWire::Type_Accepted:
		if (g_state == State_Asking || g_state == State_Pending)
			Become(State_Waiting, "accepted. You join at the start of the host's next match");
		return;
	case SpectateWire::Type_Pending:
		Become(State_Pending, "waiting for the host to let you in");
		return;
	case SpectateWire::Type_Refused:
		Become(State_Refused, RefusalText(detail));
		return;
	case SpectateWire::Type_Kicked:
		Become(State_Kicked, "the host removed you. You need their approval to watch again");
		return;
	case SpectateWire::Type_MatchStart:
		OnMatchStart(data, size);
		return;
	case SpectateWire::Type_Behind:
		g_dropped = IsJoining();
		return;
	case SpectateWire::Type_Inputs:
		OnInputs(data, size);
		return;
	case SpectateWire::Type_MatchEnd:
		if (g_state == State_Watching && g_endedAt == 0)
			g_endedAt = GetTickCount();
		return;
	default:
		return;
	}
}

bool SpectateViewer::Watch(uint64_t host)
{
	if (host == 0)
		return false;

	if (host == SteamInterfaces::GetOwnSteamId())
	{
		strncpy_s(g_status, "that is your own code", _TRUNCATE);
		return false;
	}

	if (NetLink::Lobby() != 0)
	{
		strncpy_s(g_status, "leave your room first", _TRUNCATE);
		return false;
	}

	if (!Activate(true))
	{
		strncpy_s(g_status, "watching is not supported on this game version", _TRUNCATE);
		return false;
	}

	if (g_host != 0 && g_host != host)
		Send(SpectateWire::Type_Leave);

	g_host = host;
	g_asks = 0;
	g_sentAt = 0;
	g_matchReady = false;

	Become(State_Asking, "asking the host");
	return true;
}

bool SpectateViewer::WatchCode(const char* code)
{
	uint64_t host = 0;

	if (!SpectateCode::Decode(code, host))
	{
		strncpy_s(g_status, "that is not a spectate code", _TRUNCATE);
		return false;
	}

	return Watch(host);
}

void SpectateViewer::Leave()
{
	if (g_host != 0)
		Send(SpectateWire::Type_Leave);

	g_host = 0;
	g_matchReady = false;

	Become(State_Idle, "not watching anyone");
}

SpectateViewer::State SpectateViewer::GetState()
{
	return g_state;
}

bool SpectateViewer::IsJoining()
{
	return g_state == State_Entering || g_state == State_Watching;
}

bool SpectateViewer::HoldsPatch()
{
	return g_host != 0 && (g_state == State_Waiting || IsJoining());
}

uint64_t SpectateViewer::Host()
{
	return g_host;
}

void SpectateViewer::Friends(std::vector<SteamFriends::Friend>& out)
{
	InterlockedExchange(&g_friendsWantedAt, static_cast<LONG>(GetTickCount()));

	AcquireSRWLockShared(&g_friendsLock);
	out = g_friends;
	ReleaseSRWLockShared(&g_friendsLock);
}

const char* SpectateViewer::StatusText()
{
	return g_status;
}
