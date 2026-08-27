#pragma once

#include <cstdint>

namespace RoomPing
{
	bool Initialize();

	void Update();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	uint64_t GetLobbyId();
	bool InRoom();

	int GetPublishCount();
	unsigned GetSecondsSinceLastPublish();

	const char* GetLastLocation();
	const char* GetStatusText();
}
