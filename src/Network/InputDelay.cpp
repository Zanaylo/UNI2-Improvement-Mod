#include "Network/InputDelay.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/InputDelayRule.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"

#include <cstdint>

namespace {

void* OptionAddress()
{
	return reinterpret_cast<void*>(RvaToAddress(GameOffsets::kInputDelayOption));
}

}

int InputDelay::Frames()
{
	uint8_t frames = 0;

	if (!TryReadMemory(&frames, OptionAddress(), sizeof(frames)))
		return kUnread;

	return frames;
}

bool InputDelay::CanChange()
{
	return !NetLink::InSession();
}

bool InputDelay::SetFrames(int frames)
{
	if (!InputDelayRule::MayChange(frames, NetLink::InSession()))
		return false;

	const uint8_t value = static_cast<uint8_t>(frames);

	if (!TryWriteMemory(OptionAddress(), &value, sizeof(value)))
		return false;

	LOG("InputDelay: set to %d frame(s)", frames);
	NetLog::Write("input delay: set to %d frame(s) from the overlay", frames);
	return true;
}
