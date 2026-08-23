// Whether Windows scales this window for the game or leaves it in real pixels.
//
// The game never declares a DPI awareness, so on a display set above 100% Windows renders the whole
// window at the smaller logical size and stretches the result. Everything in the frame goes through
// that second resampling, the overlay included, and no amount of internal resolution gets it back.
// Declaring awareness has to happen before the window exists and the game was not written for it,
// so it is opt-in and it reports what actually took.

#pragma once

namespace DpiScaling
{
	void Apply();

	bool IsAware();
	int GetWindowDpi();

	const char* Describe();
}
