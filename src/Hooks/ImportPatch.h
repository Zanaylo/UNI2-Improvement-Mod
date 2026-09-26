#pragma once

#include <Windows.h>

namespace ImportPatch
{
	bool Replace(HMODULE module, const char* dllName, const char* functionName, void* replacement,
		void** outOriginal);
}
