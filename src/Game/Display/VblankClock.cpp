#include "Game/Display/VblankClock.h"

#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace {

bool IsOnPrimary(HWND window)
{
	if (window == nullptr)
		return false;

	const POINT origin = { 0, 0 };
	return MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) == MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
}

double RatioHz(const UNSIGNED_RATIO& rate)
{
	return rate.uiDenominator == 0 ? 0.0 : static_cast<double>(rate.uiNumerator) / rate.uiDenominator;
}

}

bool VblankClock::Sample(HWND window)
{
	m_onPrimary = IsOnPrimary(window);

	DWM_TIMING_INFO info = {};
	info.cbSize = sizeof(info);

	if (FAILED(DwmGetCompositionTimingInfo(nullptr, &info)) || info.qpcRefreshPeriod == 0)
	{
		m_timing = {};
		m_refreshHz = 0.0;
		return false;
	}

	m_timing = { static_cast<int64_t>(info.qpcVBlank), static_cast<int64_t>(info.qpcRefreshPeriod) };
	m_refreshHz = RatioHz(info.rateRefresh);
	return true;
}

const DisplayTiming& VblankClock::Timing() const
{
	return m_timing;
}

double VblankClock::RefreshHz() const
{
	return m_refreshHz;
}

bool VblankClock::IsOnPrimaryMonitor() const
{
	return m_onPrimary;
}
