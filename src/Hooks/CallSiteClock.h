#pragma once

#include "Hooks/CallSiteScan.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace CallSiteClock
{
	constexpr size_t kThunkBytes = 160;

	using LeaveCallback = void(__cdecl*)();

	struct Clock
	{
		uint64_t start;
		uint64_t ticks;
		uint32_t calls;
		uint32_t returnTo;
		uint32_t busy;
	};

	size_t Write(uint8_t* thunk, Clock* clock, const volatile DWORD* threadId, const CallSiteScan::Site& site,
		LeaveCallback onLeave);

	bool Redirect(uint8_t* site, const CallSiteScan::Site& found, const uint8_t* thunk);
}
