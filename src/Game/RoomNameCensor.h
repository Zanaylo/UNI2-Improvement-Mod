#pragma once

namespace RoomNameCensor
{
	bool Install();
	bool IsAvailable();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	int Count();
	const char* StatusText();
}
