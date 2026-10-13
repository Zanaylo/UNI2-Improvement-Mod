#pragma once

#include "Core/SpikeReport.h"

#include <cstdint>

struct IDirect3DDevice9;

namespace SpikeWatch
{
	bool Install();
	void AttachDevice(IDirect3DDevice9* device, void** vtable);

	void Add(SpikeReport::Cost cost, int64_t ticks, uint64_t bytes = 0);
	void NoteTask(const char* group, int index, int64_t ticks);

	void OnPresent();
}
