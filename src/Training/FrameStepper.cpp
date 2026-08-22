#include "Training/FrameStepper.h"

#include "Core/Profiler.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Game/OnlineState.h"
#include "Game/ReplayState.h"
#include "Hooks/HookManager.h"
#include "Training/DummyRecorder.h"
#include "Training/FrameMeter.h"
#include "Training/StateRecorder.h"
#include "Training/PlayerControl.h"
#include "Training/StopTime.h"

#include <MinHook.h>
#include <Windows.h>

namespace {

using FrameUpdate_t = void(__fastcall*)(void*, void*);

constexpr int kDefaultStopTimeFrames = 120;
constexpr int kDefaultRefreshTicks = 60;

FrameUpdate_t oFrameUpdate = nullptr;

bool g_initialized = false;
bool g_hooked = false;
bool g_paused = false;
int g_manualResumeDelay = 0;
int g_pendingResumeDelay = 0;
int g_resumeCountdown = 0;
bool g_steppedSinceLastPresent = false;
int g_pendingSteps = 0;
int g_stepSize = 1;

FrameStepper::FreezeMode g_mode = FrameStepper::FreezeMode::TickSuppress;

int g_stopTimeFrames = kDefaultStopTimeFrames;
int g_refreshTicks = kDefaultRefreshTicks;
int g_ticksSinceApply = 0;

bool g_stopTimeHeld = false;

void ReleaseStopTime()
{
	if (!g_stopTimeHeld)
		return;

	StopTime::ApplyAll(0);
	g_stopTimeHeld = false;
}

uint64_t g_callCount = 0;
uint64_t g_suppressedFrames = 0;

void SampleObservers()
{
	{
		Profiler::Scope scope(Profiler::Section_TickMeter);
		FrameMeter::SampleFromGameThread();
	}

	Profiler::Scope scope(Profiler::Section_TickRecorder);
	StateRecorder::SampleFromGameThread();
}

void __fastcall HookedFrameUpdate(void* outputByte, void* unused)
{
	++g_callCount;

	MemoryMap::InvalidateEffectSlotCache();

	if (OnlineState::IsOnline())
	{
		g_paused = false;
		g_pendingSteps = 0;
	}

	{
		Profiler::Scope scope(Profiler::Section_TickDummy);
		DummyRecorder::Update();
	}

	const bool allowed = GameState::AllowsTrainingTools();

	if (allowed)
		StopTime::ServiceRequest();

	if (g_paused && g_resumeCountdown > 0 && allowed)
	{
		if (--g_resumeCountdown <= 0)
		{
			g_resumeCountdown = 0;
			g_paused = false;

			if (g_stopTimeHeld)
			{
				StopTime::RequestOneShot(0);
				g_stopTimeHeld = false;
			}
		}
	}

	const bool freezing = g_paused && allowed;

	if (freezing && FrameStepper::GetEffectiveMode() == FrameStepper::FreezeMode::StopTime &&
		!FrameMeter::IsSuperFlashRunning())
	{

		const bool advancing = g_pendingSteps > 0;

		if (g_pendingSteps > 0)
		{
			ReleaseStopTime();
			g_ticksSinceApply = g_refreshTicks;
			--g_pendingSteps;
			g_steppedSinceLastPresent = true;
		}
		else
		{
			if (g_ticksSinceApply >= g_refreshTicks)
			{
				if (StopTime::ApplyAll(g_stopTimeFrames))
				{
					g_ticksSinceApply = 0;
					g_stopTimeHeld = true;
				}
			}
			else
			{
				++g_ticksSinceApply;
			}

			++g_suppressedFrames;
		}

		{
			Profiler::Scope scope(Profiler::Section_TickGame);
			oFrameUpdate(outputByte, unused);
		}

		if (advancing)
		{
			SampleObservers();
		}

		Profiler::EndTickFrame();
		return;
	}

	if (freezing)
	{
		if (g_pendingSteps <= 0)
		{
			++g_suppressedFrames;

			if (outputByte != nullptr)
				*static_cast<uint8_t*>(outputByte) = 0;

			return;
		}

		ReleaseStopTime();
		--g_pendingSteps;
		g_steppedSinceLastPresent = true;
	}

	PlayerControl::OnFrameUpdate();

	{
		Profiler::Scope scope(Profiler::Section_TickGame);
		oFrameUpdate(outputByte, unused);
	}

	SampleObservers();
	Profiler::EndTickFrame();
}

}

bool FrameStepper::Initialize()
{
	if (g_initialized)
		return g_hooked;

	g_initialized = true;

	void* target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnFrameUpdate));

	if (!HookManager::CreateAndEnableHook(target, &HookedFrameUpdate,
		reinterpret_cast<void**>(&oFrameUpdate), "BattleFrameUpdate"))
	{
		return false;
	}

	g_hooked = true;
	StopTime::Initialize();
	DummyRecorder::Install();
	DummyRecorder::SetFrameCounterRva(static_cast<uintptr_t>(g_modVals.recordFrameCounterRva));

	FrameMeter::AutoPauseConfig autoPause = FrameMeter::UnpackAutoPause(g_modVals.autoPauseMode);
	for (int i = 0; i < FrameMeter::kComboStops && i < 4; ++i)
		autoPause.comboStop[i] = g_modVals.autoPauseComboStops[i];

	for (int i = 0; i < FrameMeter::kComboStops && i < 4; ++i)
		autoPause.blockStop[i] = g_modVals.autoPauseBlockStops[i];

	autoPause.resumeDelayFrames = g_modVals.resumeDelayFrames;

	FrameMeter::SetAutoPause(autoPause);

	const FreezeMode requested = g_modVals.freezeMode == 1 ? FreezeMode::StopTime
		: FreezeMode::TickSuppress;
	if (IsModeSupported(requested))
		g_mode = requested;

	LOG("FrameStepper hooked the main tick at 0x%p, stop time %s, mode %s",
		target, StopTime::IsAvailable() ? "available" : "unavailable", GetModeName(g_mode));
	return true;
}

bool FrameStepper::IsImplemented()
{
	return g_hooked;
}

bool FrameStepper::IsAvailable()
{
	return g_hooked && GameState::AllowsTrainingTools();
}

bool FrameStepper::IsPaused()
{
	return g_paused;
}

bool FrameStepper::IsFrozen()
{
	return g_paused && g_hooked && GameState::AllowsTrainingTools();
}

bool FrameStepper::SuppressesTicks()
{
	if (GetEffectiveMode() == FreezeMode::TickSuppress)
		return true;

	return FrameMeter::IsSuperFlashRunning();
}

bool FrameStepper::NeedsFrozenFrameReplay()
{
	return IsFrozen() && SuppressesTicks();
}

bool FrameStepper::ConsumeSteppedFlag()
{
	const bool stepped = g_steppedSinceLastPresent;
	g_steppedSinceLastPresent = false;
	return stepped;
}

void FrameStepper::SetPaused(bool paused)
{
	if (paused && OnlineState::IsOnline())
		return;

	if (g_paused == paused)
	{

		if (paused)
			g_resumeCountdown = 0;

		return;
	}

	if (!paused && g_pendingResumeDelay > 0)
	{
		g_resumeCountdown = g_pendingResumeDelay;
		g_pendingSteps = 0;
		return;
	}

	g_paused = paused;
	g_pendingSteps = 0;
	g_resumeCountdown = 0;

	if (paused)
	{
		g_pendingResumeDelay = g_manualResumeDelay;

		if (g_pendingResumeDelay > 0 && IsModeSupported(FreezeMode::StopTime) &&
			!ReplayState::IsPlaying())
		{
			g_mode = FreezeMode::StopTime;
		}
	}

	g_ticksSinceApply = g_refreshTicks;

	if (!paused && g_stopTimeHeld)
	{
		StopTime::RequestOneShot(0);
		g_stopTimeHeld = false;
	}
}

void FrameStepper::TogglePaused()
{
	SetPaused(!g_paused);
}

void FrameStepper::RequestStep(int frames)
{
	if (!g_paused)
		return;

	g_pendingSteps += frames > 0 ? frames : 1;
}

FrameStepper::FreezeMode FrameStepper::GetMode()
{
	return g_mode;
}

FrameStepper::FreezeMode FrameStepper::GetEffectiveMode()
{
	if (ReplayState::IsPlaying())
		return FreezeMode::TickSuppress;

	return g_mode;
}

bool FrameStepper::IsModeForced()
{
	return ReplayState::IsPlaying();
}

void FrameStepper::SetMode(FreezeMode mode)
{
	if (g_mode == mode || !IsModeSupported(mode))
		return;

	SetPaused(false);
	g_mode = mode;

	LOG("FrameStepper freeze mode set to %s", GetModeName(mode));
}

bool FrameStepper::IsModeSupported(FreezeMode mode)
{
	return mode != FreezeMode::StopTime || StopTime::IsAvailable();
}

const char* FrameStepper::GetModeName(FreezeMode mode)
{
	return mode == FreezeMode::StopTime ? "Hitstun Stop" : "Tick stop";
}

int FrameStepper::GetStopTimeFrames()
{
	return g_stopTimeFrames;
}

int FrameStepper::GetStopTimeRefreshTicks()
{
	return g_refreshTicks;
}

void FrameStepper::SetStopTimeTuning(int frames, int refreshTicks)
{
	g_stopTimeFrames = frames < 2 ? 2 : (frames > 600 ? 600 : frames);

	const int maxRefresh = g_stopTimeFrames - 1;
	g_refreshTicks = refreshTicks < 1 ? 1 : (refreshTicks > maxRefresh ? maxRefresh : refreshTicks);
}

int FrameStepper::GetStepSize()
{
	return g_stepSize;
}

void FrameStepper::SetStepSize(int frames)
{
	g_stepSize = frames < 1 ? 1 : (frames > 60 ? 60 : frames);
}

int FrameStepper::GetResumeCountdown()
{
	return g_resumeCountdown;
}

void FrameStepper::FreezeFor(int frames, FreezeMode mode)
{
	if (frames <= 0 || OnlineState::IsOnline() || !GameState::AllowsTrainingTools())
		return;

	if (IsModeSupported(mode))
		g_mode = mode;

	g_paused = true;
	g_pendingSteps = 0;
	g_pendingResumeDelay = 0;
	g_resumeCountdown = frames;
	g_ticksSinceApply = g_refreshTicks;
}

void FrameStepper::SetManualResumeDelay(int frames)
{
	g_manualResumeDelay = frames < 0 ? 0 : frames;
}

void FrameStepper::SetPausedAutomatically(int resumeDelayFrames)
{
	SetPaused(true);

	if (g_paused)
		g_pendingResumeDelay = resumeDelayFrames < 0 ? 0 : resumeDelayFrames;
}

uint64_t FrameStepper::GetSuppressedFrames()
{
	return g_suppressedFrames;
}

uint64_t FrameStepper::GetCallCount()
{
	return g_callCount;
}

void FrameStepper::Update()
{
}
