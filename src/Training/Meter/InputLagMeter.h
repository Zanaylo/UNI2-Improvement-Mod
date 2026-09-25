#pragma once

#include <cstdint>

namespace InputLagMeter
{
	enum Source
	{
		Source_Keyboard,
		Source_XInputPad,
		Source_DirectInputPad,
	};

	struct Sample
	{
		float lagMs;
		float worstGapMs;
		bool trusted;
		Source source;
	};

	constexpr int kHistory = 50;

	void KeepAlive(int targetPlayer);

	void Shutdown();
	void Reset();

	int GetCount();
	bool GetSample(int index, Sample& out);

	const char* GetSourceName(Source source);

	float GetLastMs();

	float GetAverageMs();
	int GetTrustedCount();

	int GetSampleRateHz();

	void GetWatchedDevices(int& outXInputPads, int& outDirectInputPads);
}
