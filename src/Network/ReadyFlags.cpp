#include "Network/ReadyFlags.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"

#include <cstdint>
#include <cstdio>

namespace {

constexpr int kSides = 2;
constexpr uint8_t kRaised = 1;

uintptr_t FlagAddress(int side, int phase)
{
	const intptr_t offset = static_cast<intptr_t>(side) * GameOffsets::kReadyPhasesPerSide + phase;

	return static_cast<uintptr_t>(static_cast<intptr_t>(RvaToAddress(GameOffsets::kReadyFlags)) + offset);
}

void Raise(int side, int phase)
{
	TryWriteMemory(reinterpret_cast<void*>(FlagAddress(side, phase)), &kRaised, sizeof(kRaised));
}

unsigned Flag(int side, int phase)
{
	uint8_t value = 0;
	TryReadMemory(&value, reinterpret_cast<const void*>(FlagAddress(side, phase)), sizeof(value));

	return value;
}

unsigned Dword(uintptr_t rva)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value);

	return value;
}

}

void ReadyFlags::HoldBattleStart()
{
	Raise(GameOffsets::kMatchSpectatorSide, GameOffsets::kReadyBattleStart);

	for (int side = 0; side < kSides; ++side)
		Raise(side, GameOffsets::kReadyBattleStart);
}

void ReadyFlags::Describe(char* out, int size)
{
	sprintf_s(out, size, "ready %u%u/%u%u, battle start state %u, session state %u",
		Flag(0, GameOffsets::kReadyBattleStart), Flag(1, GameOffsets::kReadyBattleStart),
		Flag(0, GameOffsets::kReadyAfterMatch), Flag(1, GameOffsets::kReadyAfterMatch),
		Dword(GameOffsets::kBattleStartState), Dword(GameOffsets::kBattleSessionState));
}
