// The engine's own hitstop, appended as a message to the battle object's queue.

#pragma once

#include <cstdint>

namespace StopTime
{
	bool Initialize();
	bool IsAvailable();

	bool ApplyAll(int frames);

	void RequestOneShot(int frames);
	void ServiceRequest();

	uint64_t GetApplyCount();
	uint64_t GetRejectCount();
	const char* GetLastRejectReason();

	bool ReadQueueState(int& outCount, int& outCapacity);
	bool ReadQueuedFrames(int& outFrames);
}
