#pragma once

namespace NotificationBar
{
	void Add(const char* format, ...);
	void Clear();

	bool HasPending();

	void Draw();
}
