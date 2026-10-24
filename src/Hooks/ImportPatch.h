#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace ImportPatch
{
	bool Replace(HMODULE module, const char* dllName, const char* functionName, void* replacement,
		void** outOriginal);

	bool NameOfSlot(HMODULE module, uintptr_t slotAddress, char* out, size_t size);
}
