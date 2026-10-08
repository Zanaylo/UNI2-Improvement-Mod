#include "Network/Spectate/SpectateRelay.h"

#include "Network/ConfirmedInputLog.h"

namespace {

static_assert(ConfirmedInputLog::kInputBytes == SpectateWire::kInputBytes, "the relay sends whole logged inputs");

ConfirmedInputLog g_log(SpectateRelay::kLogFrames);

}

void SpectateRelay::Reset()
{
	g_log.Reset();
}

void SpectateRelay::Capture(uint32_t session)
{
	g_log.Capture(session);
}

int SpectateRelay::Confirmed()
{
	return g_log.Confirmed();
}

int SpectateRelay::InputBytes()
{
	return g_log.InputBytes();
}

bool SpectateRelay::Read(int frame, uint8_t* out)
{
	return g_log.Read(frame, out);
}
