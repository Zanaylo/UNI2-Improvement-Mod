#include "Game/Stages/OnlineStage.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/ExtraStages.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Stages/StageLibrary.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>

namespace {

constexpr int kStockStage = 1;

int g_ownPick = 0;
int g_sent = 0;
int g_received = 0;
bool g_forced = false;
uint8_t g_option = 0;

char g_status[224] = "no added stage has had to be held back yet";

bool ReadStage(uintptr_t rva, int& out)
{
	uint32_t value = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value))
		return false;

	out = static_cast<int>(value);

	return true;
}

bool WriteStage(uintptr_t rva, int value)
{
	return TryWriteDword(reinterpret_cast<void*>(RvaToAddress(rva)), static_cast<uint32_t>(value));
}

uintptr_t RecordStage(int side)
{
	return GameOffsets::kMatchRecords + side * GameOffsets::kMatchRecordStride +
		GameOffsets::kMatchRecordStage;
}

bool Online()
{
	uint8_t flag = 0;

	return TryReadMemory(&flag, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kOnlineMatchFlag)),
		sizeof(flag)) && flag != 0;
}

bool Added(int stage)
{
	return stage > 0 && !StageLibrary::GameOwns(stage) && ExtraStages::RecordAt(stage) != 0;
}

void HoldOutgoing()
{
	const uintptr_t field = RecordStage(GameOffsets::kMatchRecordLocal);
	int stage = 0;

	if (!ReadStage(field, stage) || StageLibrary::GameOwns(stage))
		return;

	WriteStage(field, kStockStage);

	if (stage == g_sent)
		return;

	g_sent = stage;

	LOG("OnlineStage: stage %d is only on this machine, so the opponent is offered stage %d", stage,
		kStockStage);
}

void HoldIncoming()
{
	const uintptr_t field = RecordStage(GameOffsets::kMatchRecordRemote);
	int stage = 0;

	if (!ReadStage(field, stage) || StageLibrary::GameOwns(stage))
		return;

	WriteStage(field, kStockStage);

	if (stage == g_received)
		return;

	g_received = stage;

	LOG("OnlineStage: the opponent asked for stage %d, which this game does not have, so it plays as "
		"stage %d", stage, kStockStage);
}

void Hold()
{
	void* const option = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kStageSyncOption));
	const uint8_t own = GameOffsets::kStageSyncOwn;

	if (!g_forced && !TryReadMemory(&g_option, option, sizeof(g_option)))
		return;

	g_forced = true;
	TryWriteMemory(option, &own, sizeof(own));
}

void Release()
{
	if (!g_forced)
		return;

	void* const option = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kStageSyncOption));
	uint8_t now = 0;

	g_forced = false;
	g_ownPick = 0;

	sprintf_s(g_status, "the stage you picked is one the game ships, so both sides see the same one");

	if (TryReadMemory(&now, option, sizeof(now)) && now == GameOffsets::kStageSyncOwn)
		TryWriteMemory(option, &g_option, sizeof(g_option));
}

void ShowOwnStage(int pick)
{
	if (pick == 0)
	{
		Release();
		return;
	}

	Hold();
	WriteStage(GameOffsets::kStageOwnPick, pick);

	if (pick == g_ownPick)
		return;

	g_ownPick = pick;

	sprintf_s(g_status, "stage %d is added, so it stays on this screen and the opponent plays stage %d",
		pick, kStockStage);

	LOG("OnlineStage: %s", g_status);
}

}

void OnlineStage::OnFrame()
{
	if (!IsAddressInGameModule(RvaToAddress(GameOffsets::kMatchRecords)))
		return;

	HoldOutgoing();
	HoldIncoming();

	int pick = 0;

	if (!ReadStage(GameOffsets::kNetworkStagePick, pick))
		return;

	ShowOwnStage(Online() && Added(pick) ? pick : 0);
}

const char* OnlineStage::StatusText()
{
	return g_status;
}
