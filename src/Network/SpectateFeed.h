#pragma once

namespace SpectateFeed
{
	bool Install();
	void Reset();
	void Pump();

	int Buffered();
	int Target();
	int Stalls();
}
