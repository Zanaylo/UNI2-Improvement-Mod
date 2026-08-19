// Whether the overlay currently owns the keyboard. One latch, set on the render thread and read
// from the game and window threads, so nothing outside the overlay has to touch ImGui state across
// a thread boundary to answer it.
//
// The text-input side is fed once per overlay frame and holds for a few frames after it clears.
// The overlay draws from Present and the game reads the keyboard on its own tick, which runs
// between two of them, so a latch that dropped the instant the field let go would hand that tick
// its keys.

#pragma once

namespace KeyboardCapture
{
	void SetTextInputActive(bool active);

	// Re-asserted every frame the keybind UI is waiting for a key. DecayKeyCapture drops it when
	// that stops happening, so leaving the tab mid-capture cannot strand the latch.
	void SetKeyCaptureActive(bool active);
	void DecayKeyCapture();

	void ReleaseAll();

	bool IsKeyCaptureActive();
	bool OwnsKeyboard();
}
