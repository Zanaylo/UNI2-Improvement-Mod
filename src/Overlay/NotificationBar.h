// A line of text sliding across the top of the screen, the way BBCF Improvement Mod announces itself.
// It draws with no window open, so the mod can say it loaded the moment the game comes up.

#pragma once

namespace NotificationBar
{
	void Add(const char* format, ...);
	void Clear();

	bool HasPending();

	void Draw();
}
