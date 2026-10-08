#include "Network/RoomWatch/RoomWatchHost.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Battle/MatchBlock.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/ConfirmedInputLog.h"
#include "Network/ModChannel.h"
#include "Network/ModPresence.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/RoomWatch/HistoryPlan.h"
#include "Network/RoomWatch/RoomWatchWire.h"
#include "Network/Steam/SteamInterfaces.h"

#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

typedef int(__fastcall* AddSpectatorFn)(void*, void*, const char*, uint32_t, uint32_t, uint32_t);

constexpr int kLogFrames = 65536;
constexpr int kMostWatchers = 7;
constexpr int kHistoryWindow = 480;
constexpr int kMostQueued = 8;
constexpr DWORD kStreamEveryMs = 100;
constexpr DWORD kRewindAfterMs = 2000;
constexpr DWORD kStallMs = 15000;
constexpr DWORD kMessageTtlMs = 5000;
constexpr DWORD kHistoryTtlMs = 3000;
constexpr DWORD kAnnounceEveryMs = 2000;
constexpr DWORD kAnnounceTtlMs = 2000;
constexpr int kJoinSyncTimeoutMs = 60000;
constexpr int kNoEndpoint = -1;
constexpr int kAddFailed = -1;
constexpr int kSteamIdBits = 32;
constexpr const char* kLoopback = "127.0.0.1";

struct Watcher
{
	uint64_t id;
	int sentThrough;
	int acked;
	int firstLive;
	bool added;
	int endpoint;
	bool synchronized;
	DWORD progressAt;
	DWORD rewoundAt;
};

ConfirmedInputLog g_log(kLogFrames);
std::vector<Watcher> g_watchers;
std::vector<uint64_t> g_announced;
uint32_t g_session = 0;
bool g_hosting = false;
DWORD g_streamedAt = 0;
DWORD g_announcedAt = 0;
uint8_t g_batch[sizeof(RoomWatchWire::History) + RoomWatchWire::kMostFramesPerBatch * RoomWatchWire::kInputBytes] = {};

int ReadInt(uintptr_t address)
{
	int value = 0;
	TryReadMemory(&value, reinterpret_cast<const void*>(address), sizeof(value));

	return value;
}

int ReadGameInt(uintptr_t rva)
{
	return ReadInt(RvaToAddress(rva));
}

uint16_t ReadGameWord(uintptr_t rva)
{
	uint16_t value = 0;
	TryReadMemory(&value, reinterpret_cast<const void*>(RvaToAddress(rva)), sizeof(value));

	return value;
}

bool IsFirstInRoomOrder()
{
	return ReadGameWord(GameOffsets::kRoomLocalMember) == ReadGameWord(GameOffsets::kRoomOrder);
}

bool IsPlaying(const NetLink::Snapshot& link)
{
	return link.backend == NetLink::Backend_Players && link.layoutValid && link.hasPeer;
}

bool IsRoomMatch()
{
	return ReadGameInt(GameOffsets::kMatchKind) == GameOffsets::kMatchKindPlayer;
}

bool MayHost(const NetLink::Snapshot& link)
{
	return g_modVals.joinInProgress && IsRoomMatch() && IsPlaying(link) && IsFirstInRoomOrder();
}

void Send(uint64_t to, const void* data, int size, DWORD ttlMs, const char* label)
{
	ModChannel::SendDirectTo(to, data, size, ttlMs, label);
}

uintptr_t EndpointAddress(uintptr_t session, int index)
{
	return session + GameOffsets::kGgpoSpectatorEndpoints + static_cast<uintptr_t>(index) * GameOffsets::kGgpoEndpointStride;
}

void WriteEndpointTimeout(uintptr_t session, int index, int milliseconds)
{
	TryWriteMemory(reinterpret_cast<void*>(EndpointAddress(session, index) + GameOffsets::kGgpoEndpointSyncTimeout),
		&milliseconds, sizeof(milliseconds));
}

Watcher* Find(uint64_t id)
{
	for (Watcher& watcher : g_watchers)
	{
		if (watcher.id == id)
			return &watcher;
	}

	return nullptr;
}

bool MayAnnounceTo(const ModPresence::Member& member, uint64_t own, uint64_t opponent)
{
	return member.hasMod && member.id != 0 && member.id != own && member.id != opponent && Find(member.id) == nullptr;
}

void Announce(DWORD now)
{
	if (g_announcedAt != 0 && now - g_announcedAt < kAnnounceEveryMs)
		return;

	g_announcedAt = now;
	g_announced.clear();

	const uint64_t own = SteamInterfaces::GetOwnSteamId();
	const uint64_t opponent = NetLink::Peer();
	const RoomWatchWire::Message available = RoomWatchWire::Make(RoomWatchWire::Type_Available, 0);
	ModPresence::Member member = {};

	for (int i = 0; ModPresence::MemberAt(i, member); ++i)
	{
		if (!MayAnnounceTo(member, own, opponent))
			continue;

		Send(member.id, &available, sizeof(available), kAnnounceTtlMs, "room watch available");
		g_announced.push_back(member.id);
	}
}

void Withdraw(const RoomWatchWire::Message& leave)
{
	for (uint64_t member : g_announced)
	{
		if (Find(member) == nullptr)
			Send(member, &leave, sizeof(leave), kMessageTtlMs, "room watch withdrawn");
	}

	g_announced.clear();
	g_announcedAt = 0;
}

void StopHosting(const char* why)
{
	if (!g_hosting && g_watchers.empty())
		return;

	const RoomWatchWire::Message leave = RoomWatchWire::Make(RoomWatchWire::Type_Leave, 0);

	for (const Watcher& watcher : g_watchers)
		Send(watcher.id, &leave, sizeof(leave), kMessageTtlMs, "room watch leave");

	Withdraw(leave);

	NetLog::Write("room watch host: %s, %d watcher(s) released", why, static_cast<int>(g_watchers.size()));
	g_watchers.clear();
	g_log.Reset();
	g_hosting = false;
	g_session = 0;
}

void Accept(uint64_t from)
{
	const DWORD now = GetTickCount();
	Watcher* watcher = Find(from);

	if (watcher == nullptr)
	{
		if (static_cast<int>(g_watchers.size()) >= kMostWatchers)
		{
			NetLog::Write("room watch host: %llu asked to watch, but %d already watch", static_cast<unsigned long long>(from),
				kMostWatchers);
			return;
		}

		g_watchers.push_back({ from, -1, -1, HistoryPlan::kUnknownFrame, false, kNoEndpoint, false, now, now });
		watcher = &g_watchers.back();
	}

	RoomWatchWire::Accepted accepted = {};
	accepted.message = RoomWatchWire::Make(RoomWatchWire::Type_Accepted, 0);
	accepted.seed = ReadGameInt(GameOffsets::kMatchSeedCounter) - 1;
	accepted.confirmed = g_log.Confirmed();
	accepted.localSide = ReadGameInt(GameOffsets::kMatchLocalSide);
	accepted.remoteSide = ReadGameInt(GameOffsets::kMatchRemoteSide);
	TryReadMemory(accepted.records, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kMatchRecords)),
		sizeof(accepted.records));

	const MatchBlock::Block block = MatchBlock::Capture();
	memcpy(accepted.block, block.values, sizeof(accepted.block));

	Send(from, &accepted, sizeof(accepted), kMessageTtlMs, "room watch accepted");
	NetLog::Write("room watch host: %llu joins the match in progress, confirmed frame %d, seed %d, characters %d and %d, "
		"stage %d", static_cast<unsigned long long>(from), accepted.confirmed, accepted.seed, accepted.block[0],
		accepted.block[MatchBlock::kSlotsPerSide], accepted.block[MatchBlock::kStageSlot]);
}

void OnJoin(uint64_t from, const NetLink::Snapshot& link)
{
	if (!MayHost(link))
	{
		NetLog::Write("room watch host: %llu asked to watch, not hosting (option %d, room match %d, playing %d, first in "
			"order %d)", static_cast<unsigned long long>(from), g_modVals.joinInProgress ? 1 : 0, IsRoomMatch() ? 1 : 0,
			IsPlaying(link) ? 1 : 0, IsFirstInRoomOrder() ? 1 : 0);
		return;
	}

	if (!ModPresence::PeerHasMod(from))
		NetLog::Write("room watch host: %llu is not in this room's last scan, accepted anyway", static_cast<unsigned long long>(from));

	Accept(from);
}

int CallAddSpectator(uintptr_t session, uint64_t id)
{
	const AddSpectatorFn add = reinterpret_cast<AddSpectatorFn>(CodeSignatures::Address(GameOffsets::kFnAddSpectatorEndpoint));

	__try
	{
		return add(reinterpret_cast<void*>(session), nullptr, kLoopback, GameOffsets::kGgpoLocalPort,
			static_cast<uint32_t>(id), static_cast<uint32_t>(id >> kSteamIdBits));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return kAddFailed;
	}
}

int AddEndpoint(uintptr_t session, uint64_t id, int& cursor)
{
	const uintptr_t cursorAddress = session + GameOffsets::kGgpoSpectatorCursor;
	const uintptr_t synchronizingAddress = session + GameOffsets::kGgpoSynchronizing;

	if (ReadInt(session + GameOffsets::kGgpoSpectatorCount) == 0)
	{
		const int next = ReadInt(session + GameOffsets::kGgpoLastConfirmedFrame) + 1;
		TryWriteMemory(reinterpret_cast<void*>(cursorAddress), &next, sizeof(next));
	}

	cursor = ReadInt(cursorAddress);

	uint8_t synchronizing = 0;
	const uint8_t forced = 1;
	TryReadMemory(&synchronizing, reinterpret_cast<const void*>(synchronizingAddress), sizeof(synchronizing));
	TryWriteMemory(reinterpret_cast<void*>(synchronizingAddress), &forced, sizeof(forced));

	const int result = CallAddSpectator(session, id);

	TryWriteMemory(reinterpret_cast<void*>(synchronizingAddress), &synchronizing, sizeof(synchronizing));
	return result;
}

void OnReady(uint64_t from, const NetLink::Snapshot& link)
{
	Watcher* watcher = Find(from);

	if (watcher == nullptr || watcher->added || !MayHost(link))
		return;

	int cursor = 0;
	const int index = ReadInt(link.session + GameOffsets::kGgpoSpectatorCount);
	const int result = AddEndpoint(link.session, from, cursor);
	watcher->added = result == 0;

	if (watcher->added)
	{
		watcher->endpoint = index;
		WriteEndpointTimeout(link.session, index, kJoinSyncTimeoutMs);
	}

	RoomWatchWire::Added added = {};
	added.message = RoomWatchWire::Make(RoomWatchWire::Type_Added, 0);
	added.result = result;
	added.cursor = cursor;
	Send(from, &added, sizeof(added), kMessageTtlMs, "room watch added");

	NetLog::Write("room watch host: GGPO spectator endpoint for %llu %s (result %d), spectator cursor %d, spectators now %d",
		static_cast<unsigned long long>(from), watcher->added ? "added" : "NOT added", result, cursor,
		ReadInt(link.session + GameOffsets::kGgpoSpectatorCount));
}

void OnAck(uint64_t from, const uint8_t* data, int size)
{
	Watcher* watcher = Find(from);

	if (watcher == nullptr || size < static_cast<int>(sizeof(RoomWatchWire::Ack)))
		return;

	RoomWatchWire::Ack ack = {};
	memcpy(&ack, data, sizeof(ack));

	if (ack.contiguous > watcher->acked)
		watcher->progressAt = GetTickCount();

	watcher->acked = (std::max)(watcher->acked, static_cast<int>(ack.contiguous));

	if (ack.firstLive != HistoryPlan::kUnknownFrame && watcher->firstLive != ack.firstLive)
	{
		watcher->firstLive = ack.firstLive;
		NetLog::Write("room watch host: %llu receives live frames from %d, history stops at %d",
			static_cast<unsigned long long>(from), ack.firstLive, ack.firstLive - 1);
	}
}

void OnLeave(uint64_t from)
{
	g_watchers.erase(std::remove_if(g_watchers.begin(), g_watchers.end(),
		[from](const Watcher& watcher) { return watcher.id == from; }), g_watchers.end());

	NetLog::Write("room watch host: %llu stopped watching", static_cast<unsigned long long>(from));
}

int PackBatch(int first, int last)
{
	const int bytes = g_log.InputBytes();
	int count = 0;

	for (int frame = first; frame <= last && count < RoomWatchWire::kMostFramesPerBatch; ++frame, ++count)
	{
		if (!g_log.Read(frame, g_batch + sizeof(RoomWatchWire::History) + count * bytes))
			break;
	}

	if (count == 0)
		return 0;

	RoomWatchWire::History header = {};
	header.message = RoomWatchWire::Make(RoomWatchWire::Type_History, 0);
	header.first = first;
	header.count = static_cast<uint16_t>(count);
	header.bytes = static_cast<uint16_t>(bytes);
	memcpy(g_batch, &header, sizeof(header));

	return count;
}

void Rewind(Watcher& watcher, DWORD now)
{
	if (now - watcher.progressAt < kRewindAfterMs || now - watcher.rewoundAt < kRewindAfterMs ||
		watcher.sentThrough <= watcher.acked)
	{
		return;
	}

	watcher.sentThrough = watcher.acked;
	watcher.rewoundAt = now;
}

void StreamTo(Watcher& watcher, DWORD now)
{
	if (HistoryPlan::IsDone(watcher.acked, watcher.firstLive) || ModChannel::Queued() >= kMostQueued)
		return;

	Rewind(watcher, now);

	const int last = HistoryPlan::LastToSend(g_log.Confirmed(), watcher.acked, watcher.firstLive, kHistoryWindow);
	const int count = PackBatch(watcher.sentThrough + 1, last);

	if (count == 0)
		return;

	const int size = static_cast<int>(sizeof(RoomWatchWire::History)) + count * g_log.InputBytes();

	if (ModChannel::SendDirectTo(watcher.id, g_batch, size, kHistoryTtlMs, "room watch history"))
		watcher.sentThrough += count;
}

bool IsStalled(const Watcher& watcher, DWORD now)
{
	return !HistoryPlan::IsDone(watcher.acked, watcher.firstLive) && now - watcher.progressAt >= kStallMs;
}

void DropStalled(DWORD now)
{
	for (const Watcher& watcher : g_watchers)
	{
		if (IsStalled(watcher, now))
		{
			NetLog::Write("room watch host: %llu stopped acknowledging history at frame %d, released",
				static_cast<unsigned long long>(watcher.id), watcher.acked);
		}
	}

	g_watchers.erase(std::remove_if(g_watchers.begin(), g_watchers.end(),
		[now](const Watcher& watcher) { return IsStalled(watcher, now); }), g_watchers.end());
}

void SettleEndpoints(uintptr_t session)
{
	for (Watcher& watcher : g_watchers)
	{
		if (!watcher.added || watcher.synchronized || watcher.endpoint == kNoEndpoint)
			continue;

		const uintptr_t endpoint = EndpointAddress(session, watcher.endpoint);

		if (ReadInt(endpoint + GameOffsets::kGgpoEndpointState) != static_cast<int>(GameOffsets::kGgpoStateRunning))
			continue;

		watcher.synchronized = true;
		WriteEndpointTimeout(session, watcher.endpoint, ReadInt(session + GameOffsets::kGgpoDisconnectTimeout));
		NetLog::Write("room watch host: %llu is synchronized as a GGPO spectator, endpoint %d",
			static_cast<unsigned long long>(watcher.id), watcher.endpoint);
	}
}

void Stream(DWORD now)
{
	if (now - g_streamedAt < kStreamEveryMs || g_log.InputBytes() == 0)
		return;

	g_streamedAt = now;
	DropStalled(now);

	for (Watcher& watcher : g_watchers)
		StreamTo(watcher, now);
}

}

void RoomWatchHost::Update()
{
	const NetLink::Snapshot& link = NetLink::Current();

	if (!MayHost(link))
	{
		StopHosting("the match is over or this machine does not host it");
		return;
	}

	if (link.session != g_session)
	{
		StopHosting("a new match started");
		g_session = link.session;
	}

	if (!g_hosting)
	{
		g_hosting = true;
		NetLog::Write("room watch host: keeping this match's inputs so room members can join it in progress");
	}

	const DWORD now = GetTickCount();

	g_log.Capture(link.session);
	SettleEndpoints(link.session);
	Stream(now);
	Announce(now);
}

void RoomWatchHost::Receive(uint8_t type, const uint8_t* data, int size, uint64_t from)
{
	const NetLink::Snapshot& link = NetLink::Current();

	switch (type)
	{
	case RoomWatchWire::Type_Join:
		OnJoin(from, link);
		return;
	case RoomWatchWire::Type_Ready:
		OnReady(from, link);
		return;
	case RoomWatchWire::Type_Ack:
		OnAck(from, data, size);
		return;
	case RoomWatchWire::Type_Leave:
		OnLeave(from);
		return;
	default:
		return;
	}
}

bool RoomWatchHost::IsHosting()
{
	return g_hosting;
}

int RoomWatchHost::Watchers()
{
	return static_cast<int>(g_watchers.size());
}
