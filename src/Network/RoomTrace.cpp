#include "Network/RoomTrace.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/FieldWatch.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"

#include <cstdint>

namespace {

constexpr size_t kByte = 1;
constexpr size_t kWord = 2;
constexpr size_t kDword = 4;
constexpr size_t kRoomOrderSlots = 4;

struct GameField
{
	const char* name;
	uintptr_t rva;
	size_t bytes;
};

const GameField kFields[] = {
	{ "room task", GameOffsets::kRoomTaskState, kDword },
	{ "member choice", GameOffsets::kRoomMemberChoice, kDword },
	{ "member choice last", GameOffsets::kRoomMemberChoiceLast, kDword },
	{ "local member", GameOffsets::kRoomLocalMember, kWord },
	{ "room order count", GameOffsets::kRoomOrderCount, kByte },
	{ "room order 0", GameOffsets::kRoomOrder, kWord },
	{ "room order 1", GameOffsets::kRoomOrder + kWord, kWord },
	{ "room order 2", GameOffsets::kRoomOrder + kWord * 2, kWord },
	{ "room order 3", GameOffsets::kRoomOrder + kWord * (kRoomOrderSlots - 1), kWord },
	{ "battle start session step", GameOffsets::kBattleStartSessionStep, kDword },
	{ "battle start handshake", GameOffsets::kBattleStartHandshake, kDword },
	{ "network sub-scene", GameOffsets::kNetworkSubScene, kDword },
	{ "network sub-task", GameOffsets::kNetworkSubTask, kDword },
	{ "network exit stage", GameOffsets::kNetworkExitStage, kDword },
	{ "network exit result", GameOffsets::kNetworkExitResult, kDword },
	{ "network exit pending", GameOffsets::kNetworkExitPending, kDword },
	{ "scene", GameOffsets::kSceneId, kDword },
	{ "scene request", GameOffsets::kSceneRequest, kDword },
	{ "battle mode", GameOffsets::kBattleMode, kDword },
	{ "match kind", GameOffsets::kMatchKind, kDword },
	{ "online match flag", GameOffsets::kOnlineMatchFlag, kByte },
	{ "external input flag", GameOffsets::kExternalInputFlag, kByte },
	{ "local side", GameOffsets::kMatchLocalSide, kDword },
	{ "remote side", GameOffsets::kMatchRemoteSide, kDword },
};

constexpr int kGameFields = static_cast<int>(sizeof(kFields) / sizeof(kFields[0]));
constexpr int kBackendField = kGameFields;
constexpr int kSpectatorsField = kGameFields + 1;
constexpr int kAllFields = kGameFields + 2;

FieldWatch g_watch(kAllFields);
bool g_tracing = false;

int64_t ReadSigned(uintptr_t rva, size_t bytes)
{
	int32_t dword = 0;
	int16_t word = 0;
	int8_t byte = 0;
	const void* const at = reinterpret_cast<const void*>(RvaToAddress(rva));

	if (bytes == kByte)
		return TryReadMemory(&byte, at, sizeof(byte)) ? byte : 0;

	if (bytes == kWord)
		return TryReadMemory(&word, at, sizeof(word)) ? word : 0;

	return TryReadMemory(&dword, at, sizeof(dword)) ? dword : 0;
}

void Report(const char* name, int index, const NetLink::Snapshot& link)
{
	NetLog::Write("room trace: %s %lld -> %lld (netplay frame %d)", name,
		static_cast<long long>(g_watch.Previous(index)), static_cast<long long>(g_watch.Current(index)),
		link.netplayFrame);
}

void Observe(int index, const char* name, int64_t value, const NetLink::Snapshot& link)
{
	if (g_watch.Update(index, value))
		Report(name, index, link);
}

bool ShouldTrace(const NetLink::Snapshot& link)
{
	return NetLog::IsEnabled() && (link.lobby != 0 || link.backend != NetLink::Backend_None);
}

void Follow(bool tracing)
{
	if (tracing == g_tracing)
		return;

	g_tracing = tracing;
	g_watch.Reset();
	NetLog::Write("room trace %s", tracing ? "started" : "stopped");
}

}

void RoomTrace::Update()
{
	const NetLink::Snapshot& link = NetLink::Current();

	Follow(ShouldTrace(link));

	if (!g_tracing)
		return;

	for (int i = 0; i < kGameFields; ++i)
		Observe(i, kFields[i].name, ReadSigned(kFields[i].rva, kFields[i].bytes), link);

	Observe(kBackendField, "ggpo backend", link.backend, link);
	Observe(kSpectatorsField, "ggpo spectators", link.spectators, link);
}
