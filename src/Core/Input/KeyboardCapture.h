#pragma once

namespace KeyboardCapture
{
	void SetTextInputActive(bool active);

	void SetKeyCaptureActive(bool active);
	void DecayKeyCapture();

	void ReleaseAll();

	bool IsKeyCaptureActive();
	bool OwnsKeyboard();
}
