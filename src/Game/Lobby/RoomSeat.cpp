#include "Game/Lobby/RoomSeat.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"

#include <cstdint>

namespace {

constexpr int kSeats = 2;

bool Read(uintptr_t rva, void* out, size_t size)
{
	return TryReadMemory(out, reinterpret_cast<const void*>(RvaToAddress(rva)), size);
}

}

int RoomSeat::Local()
{
	uint8_t count = 0;
	uint16_t order[kSeats] = {};
	uint16_t local = 0;

	if (!Read(GameOffsets::kRoomOrderCount, &count, sizeof(count)) || count == 0 ||
		!Read(GameOffsets::kRoomOrder, order, sizeof(order)) ||
		!Read(GameOffsets::kRoomLocalMember, &local, sizeof(local)))
	{
		return kNone;
	}

	const int seated = count < kSeats ? count : kSeats;

	for (int seat = 0; seat < seated; ++seat)
	{
		if (order[seat] == local)
			return seat;
	}

	return kNone;
}
