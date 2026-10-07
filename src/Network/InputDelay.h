#pragma once

namespace InputDelay
{
	constexpr int kUnread = -1;

	int Frames();
	bool CanChange();
	bool SetFrames(int frames);
}
