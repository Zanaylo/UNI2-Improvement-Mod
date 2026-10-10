#pragma once

#include "Core/SpikeReport.h"

#include <cstdint>

namespace SpikeWatch
{
	bool Install();
	void AttachDevice(void** vtable);

	void Add(SpikeReport::Cost cost, int64_t ticks, uint64_t bytes = 0);
	void NoteTask(const char* group, int index, int64_t ticks);

	void OnPresent();
}
