#include "Game/Display/DisplayGrid.h"

namespace {

constexpr int64_t kHalf = 2;

bool IsUsable(const ScanlineReading& reading, const ScanoutMode& mode, int64_t slowestReadTicks)
{
	if (reading.inVerticalBlank || mode.totalLines == 0 || mode.refreshTicks <= 0)
		return false;

	const int64_t readTicks = reading.afterTicks - reading.beforeTicks;
	return readTicks >= 0 && readTicks <= slowestReadTicks && reading.line < mode.totalLines;
}

}

bool Scanout::Anchor(const ScanlineReading& reading, const ScanoutMode& mode, int64_t slowestReadTicks, int64_t& anchorTicks)
{
	if (!IsUsable(reading, mode, slowestReadTicks))
		return false;

	const int64_t middle = reading.beforeTicks + (reading.afterTicks - reading.beforeTicks) / kHalf;
	anchorTicks = middle - static_cast<int64_t>(reading.line) * mode.refreshTicks / mode.totalLines;
	return true;
}
