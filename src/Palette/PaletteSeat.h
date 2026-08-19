#pragma once

#include <cstdint>

namespace PaletteSeat
{
	constexpr int kSides = 2;
	constexpr int kMaxSeats = 8;

	struct Seat
	{
		uintptr_t owner;
		uintptr_t texture;
		int side;

		uint32_t rows;
		int draws;
		int lastSeenFrame;
	};

	void OnDraw(uintptr_t owner, uintptr_t texture, int side);

	void OnFrame();

	int GetSeatCount();
	bool GetSeat(int index, Seat& out);

	uintptr_t GetTexture(int side);

	uint32_t GetRows(int side);

	uintptr_t GetOwner(int side);
	bool GetByOwner(uintptr_t owner, uintptr_t& outTexture, uint32_t& outRows);

	int GetSideByOwner(uintptr_t owner);

	int GetFrame();

	void Reset();
}
