#include "Game/Audio/BgmLoaded.h"

#include "Core/utils.h"
#include "Game/Audio/BgmIdentity.h"
#include "Game/Engine/GameOffsets.h"

#include <cstdint>

namespace {

BgmIdentity::Binding g_binding = { BgmIdentity::kNone, BgmIdentity::kNone, 0 };

uint32_t ReadGlobal(uintptr_t rva)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value);
	return value;
}

void WriteGameId(int id)
{
	TryWriteDword(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kBgmCurrentId)), static_cast<uint32_t>(id));
}

}

int BgmLoaded::GameId()
{
	return static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId));
}

int BgmLoaded::Slot()
{
	return BgmIdentity::LoadedSlot(g_binding, GameId(), ReadGlobal(GameOffsets::kBgmPlayer));
}

void BgmLoaded::ShowSlot()
{
	WriteGameId(Slot());
}

void BgmLoaded::Present(int gameId, int slot)
{
	const uint32_t stream = ReadGlobal(GameOffsets::kBgmPlayer);

	if (stream == 0 || gameId < 0 || slot < 0)
		return;

	g_binding = { gameId, slot, stream };
	WriteGameId(gameId);
}
