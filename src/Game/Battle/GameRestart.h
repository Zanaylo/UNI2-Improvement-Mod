#pragma once

namespace GameRestart
{
	bool CanSoftReset();
	bool SoftReset();

	void OnFrame();

	bool IsPending();
	const char* StatusText();
}
