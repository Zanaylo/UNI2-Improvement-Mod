#include "D3D9/Device/SyncedVsync.h"

DWORD SyncedVsync::IntervalFor(const D3DPRESENT_PARAMETERS& parameters, bool displaySyncOn, bool wanted)
{
	if (parameters.Windowed || !displaySyncOn || !wanted)
		return parameters.PresentationInterval;

	if (parameters.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE)
		return parameters.PresentationInterval;

	return D3DPRESENT_INTERVAL_ONE;
}
