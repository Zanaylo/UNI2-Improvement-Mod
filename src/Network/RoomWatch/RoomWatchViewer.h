#pragma once

#include <cstdint>

namespace RoomWatchViewer
{
	void Update();
	void Receive(uint8_t type, const uint8_t* data, int size, uint64_t from);

	bool MatchOnOffer();
	bool WatchInProgress();
	void Leave();

	bool IsBusy();
	const char* StatusText();
}
