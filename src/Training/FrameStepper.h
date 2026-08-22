// Freezes the match two ways: TickSuppress skips the game's tick, StopTime reuses its own hitstop.
// Never reapply stop time every tick - the engine counts it down itself.

#pragma once

#include <cstdint>

namespace FrameStepper
{
	enum class FreezeMode
	{
		TickSuppress,
		StopTime
	};

	bool Initialize();
	bool IsImplemented();
	bool IsAvailable();

	void Update();

	bool IsPaused();
	bool IsFrozen();
	bool ConsumeSteppedFlag();

	bool NeedsFrozenFrameReplay();
	bool SuppressesTicks();

	void SetPaused(bool paused);
	void TogglePaused();
	void RequestStep(int frames);

	FreezeMode GetMode();
	void SetMode(FreezeMode mode);

	FreezeMode GetEffectiveMode();
	bool IsModeForced();
	bool IsModeSupported(FreezeMode mode);
	const char* GetModeName(FreezeMode mode);

	int GetStopTimeFrames();
	int GetStopTimeRefreshTicks();
	void SetStopTimeTuning(int frames, int refreshTicks);

	int GetStepSize();
	void SetStepSize(int frames);

	int GetResumeCountdown();

	void SetManualResumeDelay(int frames);
	void SetPausedAutomatically(int resumeDelayFrames);

	void FreezeFor(int frames, FreezeMode mode);

	uint64_t GetSuppressedFrames();
	uint64_t GetCallCount();
}
