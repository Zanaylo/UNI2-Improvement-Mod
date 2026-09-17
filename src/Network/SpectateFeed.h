#pragma once

#include <cstdint>

namespace SpectateFeed
{
	bool SetActive(bool active);
	void Reset();
	void Store(int first, int count, int bytes, const uint8_t* data);
	void Pump();

	int Newest();
	int Buffered();
	int Target();
	int Stalls();
}
