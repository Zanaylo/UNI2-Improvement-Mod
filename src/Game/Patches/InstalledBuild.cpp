#include "Game/Patches/InstalledBuild.h"

namespace {

constexpr uint64_t kUnixEpochIn100ns = 116444736000000000ull;
constexpr uint64_t kTicksPerSecond = 10000000ull;

uint32_t DateKey(const SYSTEMTIME& time)
{
	return time.wYear * 10000u + time.wMonth * 100u + time.wDay;
}

bool BuildDay(uint32_t buildStamp, SYSTEMTIME& out)
{
	const uint64_t ticks = kUnixEpochIn100ns + buildStamp * kTicksPerSecond;

	FILETIME file = {};
	file.dwLowDateTime = static_cast<DWORD>(ticks);
	file.dwHighDateTime = static_cast<DWORD>(ticks >> 32);

	return FileTimeToSystemTime(&file, &out) != FALSE;
}

}

bool InstalledBuild::Is(const SYSTEMTIME& released, uint32_t buildStamp)
{
	if (buildStamp == 0 || released.wYear == 0)
		return false;

	SYSTEMTIME built = {};

	if (!BuildDay(buildStamp, built))
		return false;

	return DateKey(released) >= DateKey(built);
}
