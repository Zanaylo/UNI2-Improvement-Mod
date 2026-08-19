// How long the engine takes to act on a press, sampled off the render thread - sampling at frame
// rate cannot measure a frame-scale quantity.
//
// Every device the game can read is sampled here directly rather than through the game's own poll:
// the game polls a pad once a frame, so timing from its poll would quantise every answer to 16.7 ms
// and measure nothing. The DirectInput pads are the game's own device objects, borrowed from
// InputProbe - the game acquires them exclusively, so a second device of our own would be refused.

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

	// Which devices the sampler is actually watching, for the readout to say so.
	void GetWatchedDevices(int& outXInputPads, int& outDirectInputPads);
}
