#pragma once

#include "Network/NetLink.h"

namespace RoomPing
{
	void Tick(const NetLink::Snapshot& snapshot);

	bool IsEnabled();
	void SetEnabled(bool enabled);

	int GetPublishCount();
	unsigned GetSecondsSinceLastPublish();

	void StatusText(char* out, int size);
}
