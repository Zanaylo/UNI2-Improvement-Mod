#include "Network/RoomWatch/RoomWatchViewer.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Battle/MatchBlock.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/ModChannel.h"
#include "Network/ModPresence.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/RoomWatch/RoomWatchWire.h"
#include "Network/RoomWatch/SpectatorSides.h"
#include "Network/RoomWatch/WatchOffers.h"
#include "Network/SpectatorStore.h"
#include "Network/Steam/SteamInterfaces.h"

#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

typedef void(__cdecl* RoomStepFn)();

enum class State
{
	Idle,
	Asking,
	Entering,
	Watching,
};

constexpr int kAsks = 5;
constexpr DWORD kAskEveryMs = 1000;
constexpr DWORD kReadyEveryMs = 500;
constexpr DWORD kAckEveryMs = 500;
constexpr DWORD kBarrierGraceMs = 2000;
constexpr DWORD kEnterTimeoutMs = 45000;
constexpr DWORD kMessageTtlMs = 5000;
constexpr DWORD kAckTtlMs = 1000;

State g_state = State::Idle;
uint64_t g_host = 0;
std::vector<uint64_t> g_candidates;
int g_asks = 0;
DWORD g_askedAt = 0;
DWORD g_enteredAt = 0;
DWORD g_readyAt = 0;
DWORD g_ackedAt = 0;
DWORD g_barrierSeenAt = 0;
bool g_sessionSeen = false;
bool g_endpointAdded = false;
bool g_setupFinished = false;
int g_ownRemoteSide = 0;
RoomWatchWire::Accepted g_accepted = {};
WatchOffers g_offers;
char g_status[192] = "not watching";

int ReadGameInt(uintptr_t rva)
{
	int value = 0;
	TryReadMemory(&value, reinterpret_cast<const void*>(RvaToAddress(rva)), sizeof(value));

	return value;
}

bool WriteGameInt(uintptr_t rva, int value)
{
	return TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(rva)), &value, sizeof(value));
}

bool WriteGameByte(uintptr_t rva, uint8_t value)
{
	return TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(rva)), &value, sizeof(value));
}

void Become(State state, const char* status)
{
	g_state = state;
	strncpy_s(g_status, status, _TRUNCATE);
	LOG("RoomWatchViewer: %s", g_status);
	NetLog::Write("room watch viewer: %s", g_status);
}

void SendTo(uint64_t to, RoomWatchWire::Type type)
{
	const RoomWatchWire::Message message = RoomWatchWire::Make(type, 0);
	ModChannel::SendDirectTo(to, &message, sizeof(message), kMessageTtlMs, "room watch");
}

void Stop(const char* status)
{
	if (g_host != 0)
		SendTo(g_host, RoomWatchWire::Type_Leave);

	g_host = 0;
	g_candidates.clear();
	SpectatorStore::Hold(false);
	SpectatorStore::SetActive(false);
	Become(State::Idle, status);
}

bool IsCandidate(uint64_t id)
{
	return std::find(g_candidates.begin(), g_candidates.end(), id) != g_candidates.end();
}

std::vector<uint64_t> RoomMembersWithMod()
{
	const uint64_t own = SteamInterfaces::GetOwnSteamId();
	std::vector<uint64_t> found;
	ModPresence::Member member = {};

	for (int i = 0; ModPresence::MemberAt(i, member); ++i)
	{
		if (member.hasMod && member.id != 0 && member.id != own)
			found.push_back(member.id);
	}

	return found;
}

bool IsSeated()
{
	uint16_t order[2] = {};
	uint16_t local = 0;

	TryReadMemory(order, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kRoomOrder)), sizeof(order));
	TryReadMemory(&local, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kRoomLocalMember)), sizeof(local));

	return local == order[0] || local == order[1];
}

void LogRoomState(const char* when)
{
	NetLog::Write("room watch viewer: %s: room task 0x%x, scene %d, network exit stage %d, kind %d, online %d, sides "
		"%d/%d, battle start step %d, seed %d, stage %d, characters %d and %d", when, ReadGameInt(GameOffsets::kRoomTaskState),
		ReadGameInt(GameOffsets::kSceneId), ReadGameInt(GameOffsets::kNetworkExitStage), ReadGameInt(GameOffsets::kMatchKind),
		ReadGameInt(GameOffsets::kOnlineMatchFlag) & 0xff, ReadGameInt(GameOffsets::kMatchLocalSide),
		ReadGameInt(GameOffsets::kMatchRemoteSide), ReadGameInt(GameOffsets::kBattleStartSessionStep),
		ReadGameInt(GameOffsets::kMatchSeedCounter), ReadGameInt(GameOffsets::kBgPendingNumber),
		ReadGameInt(GameOffsets::kBattleCharaRecord),
		ReadGameInt(GameOffsets::kBattleCharaRecord + GameOffsets::kBattleCharaRecordStride));
}

void LogBlock(const char* whose, const int32_t* values)
{
	NetLog::Write("room watch viewer: %s match block: characters %d/%d/%d and %d/%d/%d, stage %d, bgm %d, rules %d %d %d %d",
		whose, values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8], values[9],
		values[10], values[11]);
}

bool TakeHostSides(const RoomWatchWire::Accepted& accepted)
{
	g_ownRemoteSide = ReadGameInt(GameOffsets::kMatchRemoteSide);

	return TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kMatchRecords)), accepted.records,
		sizeof(accepted.records)) &&
		WriteGameInt(GameOffsets::kMatchLocalSide, accepted.localSide) &&
		WriteGameInt(GameOffsets::kMatchRemoteSide, accepted.remoteSide);
}

bool ReturnToSpectatorSides()
{
	const bool settingsWin = (ReadGameInt(GameOffsets::kMatchSettingsWin) & 0xff) != 0;
	const int local = GameOffsets::kMatchSpectatorSide;

	return WriteGameInt(GameOffsets::kMatchLocalSide, local) &&
		WriteGameInt(GameOffsets::kMatchRemoteSide, g_ownRemoteSide) &&
		WriteGameInt(GameOffsets::kMatchSideFlag, SpectatorSides::IsNotFirstSide(local) ? 1 : 0) &&
		WriteGameInt(GameOffsets::kMatchViewSide, SpectatorSides::ViewSide(settingsWin, local, g_ownRemoteSide));
}

bool ApplyHostBlock(const RoomWatchWire::Accepted& accepted)
{
	MatchBlock::Block block = {};
	memcpy(block.values, accepted.block, sizeof(block.values));

	LogBlock("this machine's", MatchBlock::Capture().values);
	LogBlock("the host's", block.values);

	return MatchBlock::Apply(block);
}

bool RunRoomStep(uintptr_t rva)
{
	__try
	{
		reinterpret_cast<RoomStepFn>(CodeSignatures::Address(rva))();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

bool JoinAsRoomSpectator()
{
	return RunRoomStep(GameOffsets::kFnRoomSpectatorPrepare) &&
		RunRoomStep(GameOffsets::kFnRoomRulesToBattle) &&
		RunRoomStep(GameOffsets::kFnRoomMatchStarting);
}

bool LeaveRoomScreenForBattle()
{
	return WriteGameInt(GameOffsets::kMatchKind, GameOffsets::kMatchKindPlayer) &&
		WriteGameByte(GameOffsets::kOnlineMatchFlag, 1) &&
		WriteGameInt(GameOffsets::kNetworkCommand, GameOffsets::kNetworkCommandRoomBattle) &&
		WriteGameInt(GameOffsets::kNetworkExitResult, GameOffsets::kNetworkExitResultRoom) &&
		WriteGameInt(GameOffsets::kNetworkExitStage, GameOffsets::kNetworkExitStageDispatch);
}

bool IsRoomIdle(int task)
{
	return task == GameOffsets::kRoomTaskIdle || task == GameOffsets::kRoomTaskWaitingForMatch;
}

const char* EntryBlocker()
{
	if (ReadGameInt(GameOffsets::kSceneId) != static_cast<int>(GameOffsets::kSceneNetwork))
		return "you need to be on the room screen";

	if (!IsRoomIdle(ReadGameInt(GameOffsets::kRoomTaskState)))
		return "the room is not in a match this build knows how to join";

	if (IsSeated())
		return "you are one of the players";

	return nullptr;
}

void Enter(const RoomWatchWire::Accepted& accepted)
{
	LogRoomState("before entering");

	const char* const blocker = EntryBlocker();

	if (blocker != nullptr)
	{
		Stop(blocker);
		return;
	}

	WriteGameInt(GameOffsets::kMatchSeedCounter, accepted.seed);
	g_accepted = accepted;
	g_setupFinished = false;

	if (!TakeHostSides(accepted) || !JoinAsRoomSpectator() || !LeaveRoomScreenForBattle())
	{
		Stop("the game refused the room's own spectator steps");
		return;
	}

	g_enteredAt = GetTickCount();
	g_barrierSeenAt = 0;
	g_sessionSeen = false;

	LogRoomState("after the room's spectator steps");
	Become(State::Entering, "joining the match in progress");
}

void AskToBeAdded(DWORD now)
{
	if (g_host == 0 || g_endpointAdded || (g_readyAt != 0 && now - g_readyAt < kReadyEveryMs))
		return;

	g_readyAt = now;
	SendTo(g_host, RoomWatchWire::Type_Ready);
}

void OnAccepted(uint64_t from, const uint8_t* data, int size)
{
	if (g_state != State::Asking || !IsCandidate(from) || size < static_cast<int>(sizeof(RoomWatchWire::Accepted)))
		return;

	RoomWatchWire::Accepted accepted = {};
	memcpy(&accepted, data, sizeof(accepted));

	g_host = from;

	for (uint64_t other : g_candidates)
	{
		if (other != from)
			SendTo(other, RoomWatchWire::Type_Leave);
	}

	NetLog::Write("room watch viewer: %llu hosts the match, confirmed frame %d", static_cast<unsigned long long>(from),
		accepted.confirmed);

	g_readyAt = 0;
	g_endpointAdded = false;
	AskToBeAdded(GetTickCount());
	Enter(accepted);
}

void OnAdded(uint64_t from, const uint8_t* data, int size)
{
	if (from != g_host || size < static_cast<int>(sizeof(RoomWatchWire::Added)))
		return;

	RoomWatchWire::Added added = {};
	memcpy(&added, data, sizeof(added));

	g_endpointAdded = added.result == 0;
	NetLog::Write("room watch viewer: the host %s this machine as a GGPO spectator (result %d, cursor %d)",
		g_endpointAdded ? "added" : "could NOT add", added.result, added.cursor);
}

void OnHistory(uint64_t from, const uint8_t* data, int size)
{
	if (from != g_host || size < static_cast<int>(sizeof(RoomWatchWire::History)))
		return;

	RoomWatchWire::History header = {};
	memcpy(&header, data, sizeof(header));

	const int expected = static_cast<int>(sizeof(header)) + header.count * header.bytes;

	if (header.bytes == 0 || header.bytes > RoomWatchWire::kInputBytes || size < expected)
		return;

	SpectatorStore::StoreHistory(header.first, header.count, header.bytes, data + sizeof(header));
}

void OnHostLeft(uint64_t from)
{
	if (from != g_host || g_state == State::Idle)
		return;

	if (g_state == State::Watching)
	{
		NetLog::Write("room watch viewer: the host's match ended, playing the rest from %d stored frame(s)",
			SpectatorStore::Contiguous() + 1);
		return;
	}

	Stop("the host stopped sharing the match");
}

void Ask(DWORD now)
{
	if (g_askedAt != 0 && now - g_askedAt < kAskEveryMs)
		return;

	if (g_asks >= kAsks)
	{
		Stop("the first player needs the mod with 'Let room members join my match' on");
		return;
	}

	++g_asks;
	g_askedAt = now;

	for (uint64_t candidate : g_candidates)
		SendTo(candidate, RoomWatchWire::Type_Join);
}

void SendAck(DWORD now)
{
	if (now - g_ackedAt < kAckEveryMs)
		return;

	g_ackedAt = now;

	RoomWatchWire::Ack ack = {};
	ack.message = RoomWatchWire::Make(RoomWatchWire::Type_Ack, 0);
	ack.contiguous = SpectatorStore::Contiguous();
	ack.firstLive = SpectatorStore::FirstLive();
	ModChannel::SendDirectTo(g_host, &ack, sizeof(ack), kAckTtlMs, "room watch ack");
}

void ForceBattleReady()
{
	const int pad = ReadGameInt(GameOffsets::kNetplayPadSlot);

	WriteGameByte(GameOffsets::kExternalInputFlag, 1);
	WriteGameInt(GameOffsets::kExternalInputExtra, 0);
	WriteGameInt(GameOffsets::kNetplayPadSlotA, pad);
	WriteGameInt(GameOffsets::kNetplayPadOwnerA, 0);
	WriteGameInt(GameOffsets::kNetplayPadOwnerB, 1);
	WriteGameInt(GameOffsets::kNetplayPadSlotB, pad != 0 ? 1 : 0);
	WriteGameInt(GameOffsets::kRoomTaskState, GameOffsets::kRoomTaskBattle);
}

void PassBarrier(DWORD now)
{
	if (ReadGameInt(GameOffsets::kRoomTaskState) != GameOffsets::kRoomTaskBarrier)
	{
		g_barrierSeenAt = 0;
		return;
	}

	if (g_barrierSeenAt == 0)
	{
		g_barrierSeenAt = now;
		LogRoomState("the room reached its last barrier");
		return;
	}

	if (now - g_barrierSeenAt < kBarrierGraceMs)
		return;

	ForceBattleReady();
	LogRoomState("the room's last barrier did not open for a late spectator, passed it the way the game does");
	g_barrierSeenAt = 0;
}



void FinishSetup()
{
	if (g_setupFinished || ReadGameInt(GameOffsets::kSceneId) != GameOffsets::kSceneRoomBattleStart)
		return;

	g_setupFinished = true;
	LogRoomState("the game set the match up with the host's sides");

	const bool sides = ReturnToSpectatorSides();
	const bool block = ApplyHostBlock(g_accepted);

	LogRoomState(sides && block ? "back to spectator sides, the host's match block applied" :
		"the spectator sides or the host's match block could not be written");
}

void Arrive(DWORD now)
{
	const NetLink::Snapshot& link = NetLink::Current();

	FinishSetup();
	PassBarrier(now);
	AskToBeAdded(now);
	SendAck(now);

	if (link.backend == NetLink::Backend_Spectator && !link.synchronizing && g_endpointAdded)
	{
		Become(State::Watching, "watching the match in progress");
		return;
	}

	if (now - g_enteredAt >= kEnterTimeoutMs)
	{
		LogRoomState("gave up entering");
		Stop("the match did not start here in time");
	}
}

void Follow(DWORD now)
{
	const NetLink::Snapshot& link = NetLink::Current();

	SendAck(now);

	if (link.backend == NetLink::Backend_Spectator)
	{
		g_sessionSeen = true;
		return;
	}

	if (!g_sessionSeen)
		return;

	LogRoomState("the spectator session ended");
	Stop("the match ended");
}

}

void RoomWatchViewer::Update()
{
	const DWORD now = GetTickCount();

	switch (g_state)
	{
	case State::Asking:
		Ask(now);
		return;
	case State::Entering:
		Arrive(now);
		return;
	case State::Watching:
		Follow(now);
		return;
	default:
		return;
	}
}

void RoomWatchViewer::Receive(uint8_t type, const uint8_t* data, int size, uint64_t from)
{
	switch (type)
	{
	case RoomWatchWire::Type_Accepted:
		OnAccepted(from, data, size);
		return;
	case RoomWatchWire::Type_Added:
		OnAdded(from, data, size);
		return;
	case RoomWatchWire::Type_History:
		OnHistory(from, data, size);
		return;
	case RoomWatchWire::Type_Leave:
		g_offers.Forget(from);
		OnHostLeft(from);
		return;
	case RoomWatchWire::Type_Available:
		g_offers.Note(from, GetTickCount());
		return;
	default:
		return;
	}
}

bool RoomWatchViewer::MatchOnOffer()
{
	return g_offers.AnyFresh(GetTickCount());
}

bool RoomWatchViewer::WatchInProgress()
{
	if (g_state != State::Idle)
		return false;

	if (NetLink::Lobby() == 0 || !ModPresence::InRoom())
	{
		strncpy_s(g_status, "you are not in a room", _TRUNCATE);
		return false;
	}

	if (NetLink::Current().backend != NetLink::Backend_None)
	{
		strncpy_s(g_status, "you are already in a match", _TRUNCATE);
		return false;
	}

	std::vector<uint64_t> candidates = RoomMembersWithMod();

	if (candidates.empty())
	{
		strncpy_s(g_status, "nobody else in this room runs the mod", _TRUNCATE);
		return false;
	}

	if (!SpectatorStore::SetActive(true))
	{
		strncpy_s(g_status, "watching is not supported on this game version", _TRUNCATE);
		return false;
	}

	SpectatorStore::Reset();
	SpectatorStore::Hold(true);
	g_candidates.swap(candidates);
	g_host = 0;
	g_asks = 0;
	g_askedAt = 0;
	g_ackedAt = 0;

	LogRoomState("asked to watch");
	Become(State::Asking, "asking the first player");
	return true;
}

void RoomWatchViewer::Leave()
{
	if (g_state == State::Idle)
		return;

	Stop("not watching");
}

bool RoomWatchViewer::IsBusy()
{
	return g_state != State::Idle;
}

const char* RoomWatchViewer::StatusText()
{
	return g_status;
}
