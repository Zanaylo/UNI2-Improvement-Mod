#pragma once

namespace RollbackStats
{
	constexpr int kLiveSamples = 600;
	constexpr int kStartSamples = 900;

	struct Sample
	{
		int frame;
		int resimulated;
		float resimulatedPerSecond;
		int ping;
		int localFramesBehind;
		int remoteFramesBehind;
		int sendQueue;
		int kbpsSent;
		float frameMs;
	};

	void Update();

	bool IsNetplayActive();
	bool HasSession();

	int GetFrame();
	int GetResimulatedTotal();
	float GetResimulatedPerSecond();

	const Sample& GetLatest();

	int LiveCount();
	const Sample& Live(int index);

	int StartCount();
	const Sample& Start(int index);
	bool StartCaptureComplete();
	void ClearStartCapture();

	const char* GetStatusText();
}
