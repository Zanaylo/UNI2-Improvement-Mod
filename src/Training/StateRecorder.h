// Records per-frame character state to CSV, so field meanings are derived from data not guessed.

#pragma once

#include <cstdint>

namespace StateRecorder
{
	bool IsRecording();

	void Start(bool includeDeltas);
	void Stop();

	void SampleFromGameThread();

	bool IncludesDeltas();
	bool IsBufferFull();
	int GetSampledFrames();
	int GetRecordCount();
	int GetCapacity();
	int GetTrackedEntities();
	const char* GetLastFilePath();
}
