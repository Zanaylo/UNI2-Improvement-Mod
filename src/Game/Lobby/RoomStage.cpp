#include "Game/Lobby/RoomStage.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Stages/ExtraStages.h"
#include "Game/Stages/StageLibrary.h"
#include "Hooks/GameHook.h"
#include "Network/NetLog.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

typedef void(__thiscall* MemberRecordFn)(void* record, void* cursor, char extended);

GameHook<MemberRecordFn> g_writeHook("RoomMemberWrite");
GameHook<MemberRecordFn> g_readHook("RoomMemberRead");

volatile long g_heldOutgoing = 0;
volatile long g_heldIncoming = 0;
int g_lastOutgoing = 0;
int g_lastIncoming = 0;

volatile long g_writes = 0;
volatile long g_reads = 0;

char g_status[192] = "not installed";

void NoteFirst(volatile long& calls, const char* what, int stage)
{
	if (InterlockedIncrement(&calls) != 1)
		return;

	LOG("RoomStage: first room member record %s, stage %d", what, stage);
	NetLog::Write("RoomStage: first room member record %s, stage %d", what, stage);
}

int* StageField(void* record)
{
	return reinterpret_cast<int*>(static_cast<char*>(record) + GameOffsets::kRoomMemberStage);
}

bool Shareable(int stage)
{
	return StageLibrary::GameOwns(stage);
}

bool Loadable(int stage)
{
	return StageLibrary::GameOwns(stage) || ExtraStages::RecordAt(stage) != 0;
}

void Report(const char* format, int stage, int& last)
{
	if (stage == last)
		return;

	last = stage;
	LOG(format, stage, StageLibrary::kStockStage);
	NetLog::Write(format, stage, StageLibrary::kStockStage);
}

void __fastcall HookedWrite(void* record, void* unused, void* cursor, char extended)
{
	int* const field = StageField(record);
	const int stage = *field;
	NoteFirst(g_writes, "sent", stage);

	if (Shareable(stage))
	{
		g_writeHook.Original()(record, cursor, extended);
		return;
	}

	*field = StageLibrary::kStockStage;
	g_writeHook.Original()(record, cursor, extended);
	*field = stage;

	InterlockedIncrement(&g_heldOutgoing);
	Report("RoomStage: stage %d is only on this machine, so the room is told stage %d", stage, g_lastOutgoing);
}

void __fastcall HookedRead(void* record, void* unused, void* cursor, char extended)
{
	g_readHook.Original()(record, cursor, extended);

	int* const field = StageField(record);
	const int stage = *field;
	NoteFirst(g_reads, "received", stage);

	if (Loadable(stage))
		return;

	*field = StageLibrary::kStockStage;

	InterlockedIncrement(&g_heldIncoming);
	Report("RoomStage: a room member asked for stage %d, which this game does not have, so it is stage %d",
		stage, g_lastIncoming);
}

bool InstallHook(GameHook<MemberRecordFn>& hook, uintptr_t rva, MemberRecordFn handler)
{
	void* const target = reinterpret_cast<void*>(CodeSignatures::Address(rva));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
		return false;

	return hook.Install(target, handler);
}

}

bool RoomStage::Install()
{
	if (g_writeHook.IsLive() && g_readHook.IsLive())
		return true;

	const bool write = InstallHook(g_writeHook, GameOffsets::kFnRoomMemberWrite,
		reinterpret_cast<MemberRecordFn>(&HookedWrite));
	const bool read = InstallHook(g_readHook, GameOffsets::kFnRoomMemberRead,
		reinterpret_cast<MemberRecordFn>(&HookedRead));

	if (!write || !read)
	{
		sprintf_s(g_status, "the room member code is not where this game version expects it (write %d, read %d)",
			write, read);
		LOG("RoomStage: %s", g_status);
		return false;
	}

	strncpy_s(g_status, "on, added stages never reach another room member", _TRUNCATE);
	LOG("RoomStage: %s", g_status);
	return true;
}

const char* RoomStage::StatusText()
{
	if (!g_writeHook.IsLive())
		return g_status;

	sprintf_s(g_status, "on, %ld added stage%s held back from the room, %ld unknown stage%s from it replaced",
		g_heldOutgoing, g_heldOutgoing == 1 ? "" : "s", g_heldIncoming, g_heldIncoming == 1 ? "" : "s");
	return g_status;
}
