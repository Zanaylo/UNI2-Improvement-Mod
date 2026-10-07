#pragma once

namespace RoundTripSmoothing
{
	bool Install();
	void OnFrame();

	bool IsInstalled();
	bool IsPrimed();
	double SmoothedMs();
	int Frames();
}
