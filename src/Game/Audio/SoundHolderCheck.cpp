#include "Game/Audio/SoundHolderCheck.h"

bool SoundHolderCheck::BufferAlive(bool readable, uint32_t vtable, uint32_t soundBufferVtable)
{
	if (!readable)
		return false;

	return soundBufferVtable == 0 || vtable == soundBufferVtable;
}
