#pragma once

namespace ScreenShake
{
	constexpr int kFullPercent = 100;

	bool Install();

	bool IsAvailable();

	int GetIntensity();
	void SetIntensity(int percent);

	const char* StatusText();
}
