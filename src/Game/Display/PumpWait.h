#pragma once

#include <Windows.h>

namespace PumpWait
{
	bool Install();
	void Apply();
	void Shutdown();

	bool IsActive();

	bool GetWaitStats(double& outMedianUs, double& outP99Us, int& outSamples);

	void GetReturnCounts(unsigned& outSignalled, unsigned& outTimedOut, unsigned& outPassedThrough);
}
