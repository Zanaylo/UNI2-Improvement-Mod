#include "Palette/EffectBuffer.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Game/Engine/GameState.h"
#include "Palette/EffectOwner.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteControl.h"
#include "Palette/PalettePaint.h"
#include "Palette/TintBuffer.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>

namespace {

constexpr int kPlayers = 2;
constexpr int kBytesPerEntry = 4;
constexpr int kAlpha = 3;
constexpr uint8_t kMarked = 255;

TintBuffer g_buffers[kPlayers];
bool g_reported[kPlayers] = {};

bool IsInsideTheGame(uintptr_t address)
{
	static const uintptr_t start = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
	static const IMAGE_NT_HEADERS* const headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(
		start + reinterpret_cast<const IMAGE_DOS_HEADER*>(start)->e_lfanew);
	static const uintptr_t end = start + headers->OptionalHeader.SizeOfImage;

	return address >= start && address + TintBuffer::kBytes <= end;
}

uint8_t* BufferOf(int player)
{
	const uintptr_t address = EffectOwner::GetTintPalette(player);

	return IsInsideTheGame(address) ? reinterpret_cast<uint8_t*>(address) : nullptr;
}

void ReadOverlay(int player, uint8_t* overlay)
{
	memset(overlay, 0, TintBuffer::kBytes);

	for (int entry = 1; entry < TintBuffer::kColours; ++entry)
	{
		uint8_t* const at = overlay + entry * kBytesPerEntry;

		if (EffectPaint::GetRemoteEntry(player, entry, at) || EffectPaint::GetEdit(player, entry, at))
			at[kAlpha] = kMarked;
	}
}

void Report(int player, const uint8_t* live)
{
	if (g_reported[player])
		return;

	g_reported[player] = true;

	LOG("effect buffer: p%d wrote the palette into 0x%08x", player, static_cast<unsigned>(reinterpret_cast<uintptr_t>(live)));
}

}

void EffectBuffer::OnFrame()
{
	const bool usable = g_modVals.paletteEffectsFromPalette && GameState::IsInMatch() && !PalettePaint::IsAtPreview();

	for (int player = 0; player < kPlayers; ++player)
	{
		if (!usable || !PaletteControl::CanWear(player))
		{
			g_buffers[player].Restore();
			continue;
		}

		uint8_t overlay[TintBuffer::kBytes];

		ReadOverlay(player, overlay);

		uint8_t* const live = BufferOf(player);

		if (g_buffers[player].Apply(live, PalettePaint::GetWearing(player), overlay))
			Report(player, live);
	}
}
