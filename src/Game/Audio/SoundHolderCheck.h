#pragma once

#include <cstdint>

namespace SoundHolderCheck
{
	bool BufferAlive(bool readable, uint32_t vtable, uint32_t soundBufferVtable);
}
