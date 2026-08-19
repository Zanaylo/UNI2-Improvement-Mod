// Whether a replay is playing back. Exists because Hitstun Stop cannot pause a replay.

#pragma once

#include <cstdint>

namespace ReplayState
{
	void Update();

	bool IsPlaying();

	bool IsDetectionReady();

	const char* GetStatusText();

	struct Signal
	{
		const char* name;
		uintptr_t rva;
		int width;
		uint32_t value;
		bool read;
	};

	int GetSignalCount();
	bool GetSignal(int index, Signal& outSignal);
}
