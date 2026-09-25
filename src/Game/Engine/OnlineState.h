#pragma once

namespace OnlineState
{
	void Update();

	bool IsOnline();
	bool IsDetectionReady();
	bool HasSession();
	bool IsNetplay();
	bool IsSpectating();
	bool IsBlind();

	const char* GetStatusText();
}
