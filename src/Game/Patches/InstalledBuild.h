#pragma once

#include <Windows.h>

#include <cstdint>

namespace InstalledBuild
{
	bool Is(const SYSTEMTIME& released, uint32_t buildStamp);
}
