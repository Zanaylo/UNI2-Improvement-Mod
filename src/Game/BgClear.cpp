#include "Game/BgClear.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/StageLibrary.h"

#include <Windows.h>

namespace {

constexpr uint32_t kStageGreen = 0xff006400u;
constexpr uint32_t kNight = 0xff000000u;

volatile long g_ported = 0;
int g_stage = -1;

}

void BgClear::Update()
{
	const uintptr_t address = RvaToAddress(GameOffsets::kBgPendingNumber);

	if (!IsAddressInGameModule(address))
		return;

	const int number = *reinterpret_cast<const int*>(address);
	const int loaded = StageLibrary::IdForSlot(number);
	const int stage = loaded < 0 ? number : loaded;

	if (stage == g_stage)
		return;

	g_stage = stage;

	StageLibrary::Entry entry = {};
	const bool ported = StageLibrary::Of(stage, entry);

	InterlockedExchange(&g_ported, ported ? 1 : 0);

	if (ported)
		LOG("BgClear: stage %d is a port, so the frame behind it is cleared to black rather than "
			"the game's green", stage);
}

uint32_t BgClear::Behind(uint32_t colour)
{
	if (colour != kStageGreen)
		return colour;

	if (InterlockedCompareExchange(&g_ported, 0, 0) == 0)
		return colour;

	return kNight;
}
