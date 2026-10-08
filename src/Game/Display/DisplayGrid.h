#pragma once

#include <cstdint>

struct DisplayGrid
{
	int64_t anchorTicks;
	int64_t refreshTicks;
};

struct ScanoutMode
{
	int64_t refreshTicks;
	uint32_t totalLines;
};

struct ScanlineReading
{
	int64_t beforeTicks;
	int64_t afterTicks;
	uint32_t line;
	bool inVerticalBlank;
};

namespace Scanout
{
	bool Anchor(const ScanlineReading& reading, const ScanoutMode& mode, int64_t slowestReadTicks, int64_t& anchorTicks);
}
