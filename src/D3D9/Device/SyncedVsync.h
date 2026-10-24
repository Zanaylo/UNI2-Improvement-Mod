#pragma once

#include <d3d9.h>

namespace SyncedVsync
{
	DWORD IntervalFor(const D3DPRESENT_PARAMETERS& parameters, bool displaySyncOn, bool wanted);
}
