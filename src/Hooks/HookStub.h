#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace HookStub
{
	constexpr size_t kBytes = 32;

	void Write(uint8_t* stub, void* volatile* gate, volatile LONG* counter, void* handler);
	void* CountingEntry(uint8_t* stub);
	bool Contains(const uint8_t* stub, const void* address);
}
