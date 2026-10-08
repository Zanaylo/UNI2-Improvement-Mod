#pragma once

#include <cstdint>

namespace SpectatorStore
{
	bool SetActive(bool active);
	bool IsActive();
	void Reset();
	void StartAt(int frame);

	void Hold(bool held);
	bool IsHeld();

	void StoreHistory(int first, int count, int bytes, const uint8_t* data);

	int Contiguous();
	int FirstLive();
	int ReadyFrom(int next);
}
