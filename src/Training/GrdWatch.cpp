#include "Training/GrdWatch.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"

#include <cstdint>

namespace {

constexpr int kQuietFrames = 6;
constexpr int kMaxBurstFrames = 40;

struct Burst
{
	int last = 0;
	bool hasLast = false;

	int side = 0;
	int pending = 0;
	int quiet = 0;
	int length = 0;
};

Burst g_bursts[GrdWatch::kPlayers] = {};
GrdWatch::Popup g_popups[GrdWatch::kMaxPopups] = {};
int g_popupCount = 0;

uint32_t g_lastCounter = 0;
bool g_hasCounter = false;

bool BattleFrameAdvanced()
{
	uint32_t counter = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kFrameCounterA)), counter))
		return false;

	const bool restarted = g_hasCounter && counter < g_lastCounter;
	const bool advanced = !g_hasCounter || counter != g_lastCounter;

	if (restarted)
		GrdWatch::Reset();

	g_hasCounter = true;
	g_lastCounter = counter;

	return advanced && !restarted;
}

void Emit(int side, int amount)
{
	if (g_popupCount == GrdWatch::kMaxPopups)
	{
		for (int i = 1; i < g_popupCount; ++i)
			g_popups[i - 1] = g_popups[i];

		--g_popupCount;
	}

	g_popups[g_popupCount++] = { side, amount, 0 };
}

void AgePopups()
{
	int kept = 0;

	for (int i = 0; i < g_popupCount; ++i)
	{
		if (++g_popups[i].age >= GrdWatch::kLifeFrames)
			continue;

		g_popups[kept++] = g_popups[i];
	}

	g_popupCount = kept;
}

void Close(Burst& burst)
{
	if (burst.pending != 0)
		Emit(burst.side, burst.pending);

	burst.pending = 0;
	burst.quiet = 0;
	burst.length = 0;
}

void Track(int player)
{
	Burst& burst = g_bursts[player];

	int side = 0;
	int units = 0;
	if (!GrdWatch::ReadGrd(player, side, units))
		return;

	const int delta = burst.hasLast && side == burst.side ? units - burst.last : 0;

	burst.side = side;

	burst.last = units;
	burst.hasLast = true;

	if (delta == 0 && burst.pending == 0)
		return;

	if (delta != 0)
	{
		burst.pending += delta;
		burst.quiet = 0;
	}
	else
	{
		++burst.quiet;
	}

	++burst.length;

	if (burst.quiet >= kQuietFrames || burst.length >= kMaxBurstFrames)
		Close(burst);
}

}

bool GrdWatch::ReadGrd(int player, int& outSide, int& outUnits)
{
	void* const chara = MemoryMap::GetCharaSlot(player);
	if (chara == nullptr)
		return false;

	uint32_t sideWord = 0;
	if (!MemoryMap::ReadStructDword(chara, GameOffsets::kCharaSideIndex, sideWord))
		return false;

	const uint32_t side = sideWord & 0xff;
	if (side >= kPlayers)
		return false;

	const uintptr_t gauge = RvaToAddress(GameOffsets::kGrdGaugeBase) + side * GameOffsets::kGrdGaugeStride;

	uint32_t blocks = 0;
	uint32_t partial = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(gauge + GameOffsets::kGrdGaugeBlocks), blocks) ||
		!TryReadDword(reinterpret_cast<const void*>(gauge + GameOffsets::kGrdGaugePartial), partial))
	{
		return false;
	}

	if (blocks > GameOffsets::kGrdGaugeMaxBlocks || partial >= static_cast<uint32_t>(kUnitsPerBlock))
		return false;

	outSide = static_cast<int>(side);
	outUnits = static_cast<int>(blocks) * kUnitsPerBlock + static_cast<int>(partial);
	return true;
}

bool GrdWatch::IsAllowedHere()
{
	return GameState::AllowsTrainingTools() && GameState::IsTrainingBattle();
}

bool GrdWatch::IsAllowed()
{
	return g_modVals.grdPopups && IsAllowedHere();
}

bool GrdWatch::ReadCycleRemaining(int& outFrames)
{
	uint32_t elapsed = 0;
	uint32_t length = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kGrdCycleElapsed)), elapsed) ||
		!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kGrdCycleLength)), length))
	{
		return false;
	}

	if (length == 0 || elapsed > length)
		return false;

	outFrames = static_cast<int>((length - elapsed + GameOffsets::kGrdCycleUnitsPerFrame - 1) /
		GameOffsets::kGrdCycleUnitsPerFrame);
	return true;
}

void GrdWatch::SampleFromGameThread()
{
	if (!IsAllowed())
	{
		if (g_popupCount != 0 || g_hasCounter)
			Reset();

		return;
	}

	if (!BattleFrameAdvanced())
		return;

	AgePopups();

	for (int player = 0; player < kPlayers; ++player)
		Track(player);
}

void GrdWatch::Reset()
{
	for (Burst& burst : g_bursts)
		burst = Burst();

	g_popupCount = 0;
	g_hasCounter = false;
}

int GrdWatch::GetPopups(Popup* out, int max)
{
	int count = 0;

	for (int i = 0; i < g_popupCount && count < max; ++i)
		out[count++] = g_popups[i];

	return count;
}
