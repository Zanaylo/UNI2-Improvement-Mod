#pragma once

#include "Game/Display/PhaseLock.h"

#include <Windows.h>

class VblankClock
{
public:
	bool Sample(HWND window);

	const DisplayTiming& Timing() const;
	double RefreshHz() const;
	bool IsOnPrimaryMonitor() const;

private:
	DisplayTiming m_timing = {};
	double m_refreshHz = 0.0;
	bool m_onPrimary = false;
};
