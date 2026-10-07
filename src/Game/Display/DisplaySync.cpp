#include "Game/Display/DisplaySync.h"

#include "Core/Boot/Compat.h"
#include "Core/CodeFingerprint.h"
#include "Core/Config/interfaces.h"
#include "Core/MeasuredCode.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Display/PhaseLock.h"
#include "Game/Display/PhaseStats.h"
#include "Game/Display/VblankClock.h"
#include "Game/Engine/GameOffsets.h"

#include <Windows.h>

#include <cmath>
#include <cstdio>

namespace {

constexpr double kGain = 0.1;
constexpr double kMaxStepMs = 0.25;
constexpr double kMaxRateMismatch = 0.002;
constexpr double kOnTargetWithinMs = 0.5;
constexpr double kMillisecondsPerSecond = 1000.0;
constexpr int kClockSampleFrames = 30;
constexpr int kReportFrames = 600;
constexpr int kPercent = 100;

const CodeFingerprint kLimiterCode[] = {
	{ GameOffsets::kSiteFrameWaitHasQpcRead, { 0x83, 0x3D }, 2, GameOffsets::kFrameWaitHasQpc, { 0x00 }, 1 },
	{ GameOffsets::kSiteFrameWaitFrozenRead, { 0x8B, 0x0D }, 2, GameOffsets::kFrameWaitFrozenNow, {}, 0 },
	{ GameOffsets::kSiteFrameWaitPeriodRead, { 0xF3, 0x0F, 0x10, 0x15 }, 4, GameOffsets::kFramePeriodSeconds, {}, 0 },
	{ GameOffsets::kSiteFrameWaitLoopStartRead, { 0x2B, 0x0D }, 2, GameOffsets::kFrameWaitStart, {}, 0 },
	{ GameOffsets::kSiteFrameWaitStartWrite, { 0x89, 0x0D }, 2, GameOffsets::kFrameWaitStart, {}, 0 },
};

enum class State
{
	Off,
	UnknownGameCode,
	GameClockPaused,
	NoDisplayClock,
	OtherMonitor,
	RateMismatch,
	Locking,
};

enum class Readiness
{
	Unchecked,
	Ready,
	Refused,
};

struct Limiter
{
	int64_t waitEnd;
	int64_t framePeriodTicks;
};

Readiness g_readiness = Readiness::Unchecked;
State g_state = State::Off;
VblankClock g_clock;
int g_framesSinceSample = kClockSampleFrames;
int g_onTargetPercent = 0;
bool g_hasReport = false;
char g_status[160] = "Off.";

int64_t Frequency()
{
	static const int64_t frequency = []
	{
		LARGE_INTEGER value = {};
		QueryPerformanceFrequency(&value);
		return value.QuadPart;
	}();

	return frequency;
}

int64_t MsToTicks(double milliseconds)
{
	return static_cast<int64_t>(std::llround(milliseconds * Frequency() / kMillisecondsPerSecond));
}

double TicksToMs(int64_t ticks)
{
	return static_cast<double>(ticks) * kMillisecondsPerSecond / Frequency();
}

PhaseLock& Lock()
{
	static PhaseLock lock({ kGain, MsToTicks(kMaxStepMs), kMaxRateMismatch });
	return lock;
}

PhaseStats& Stats()
{
	static PhaseStats stats(MsToTicks(kOnTargetWithinMs));
	return stats;
}

void* GameAddress(uintptr_t rva)
{
	return reinterpret_cast<void*>(RvaToAddress(rva));
}

bool LimiterIsMeasured()
{
	if (g_readiness != Readiness::Unchecked)
		return g_readiness == Readiness::Ready;

	const int mismatch = MeasuredCode::FirstMismatch(kLimiterCode);
	g_readiness = mismatch == MeasuredCode::kAllMatch ? Readiness::Ready : Readiness::Refused;

	if (g_readiness == Readiness::Ready)
		LOG("DisplaySync: the frame limiter matches the measured 1.40 code");
	else
		LOG("DisplaySync: rva 0x%x is not the measured 1.40 code, frames keep the game's own timing",
			static_cast<unsigned>(kLimiterCode[mismatch].siteRva));

	return g_readiness == Readiness::Ready;
}

bool ReadLimiter(Limiter& out)
{
	uint32_t hasQpc = 0;
	if (!TryReadDword(GameAddress(GameOffsets::kFrameWaitHasQpc), hasQpc) || hasQpc == 0)
		return false;

	int64_t frozenNow = 0;
	if (!TryReadMemory(&frozenNow, GameAddress(GameOffsets::kFrameWaitFrozenNow), sizeof(frozenNow)) ||
		frozenNow != 0)
	{
		return false;
	}

	float periodSeconds = 0.0f;
	if (!TryReadMemory(&periodSeconds, GameAddress(GameOffsets::kFramePeriodSeconds), sizeof(periodSeconds)) ||
		periodSeconds <= 0.0f)
	{
		return false;
	}

	if (!TryReadMemory(&out.waitEnd, GameAddress(GameOffsets::kFrameWaitStart), sizeof(out.waitEnd)))
		return false;

	out.framePeriodTicks = static_cast<int64_t>(std::llround(periodSeconds * Frequency()));
	return true;
}

HWND GameWindow()
{
	uint32_t handle = 0;
	TryReadDword(GameAddress(GameOffsets::kWindowHandle), handle);
	return reinterpret_cast<HWND>(static_cast<uintptr_t>(handle));
}

bool SampleClockWhenDue()
{
	if (++g_framesSinceSample < kClockSampleFrames)
		return g_clock.Timing().refreshTicks > 0;

	g_framesSinceSample = 0;
	return g_clock.Sample(GameWindow());
}

void Describe(State state)
{
	switch (state)
	{
	case State::Off:
		sprintf_s(g_status, "Off.");
		break;
	case State::UnknownGameCode:
		sprintf_s(g_status, "Not available for this version of the game.");
		break;
	case State::GameClockPaused:
		sprintf_s(g_status, "Waiting for the game.");
		break;
	case State::NoDisplayClock:
		sprintf_s(g_status, "Windows did not report the screen's timing.");
		break;
	case State::OtherMonitor:
		sprintf_s(g_status, "Only works on the main monitor.");
		break;
	case State::RateMismatch:
		sprintf_s(g_status, "Your screen runs at %.2f Hz, which cannot show 60 frames evenly.", g_clock.RefreshHz());
		break;
	case State::Locking:
		if (g_hasReport)
			sprintf_s(g_status, "On: %.2f Hz screen, %d%% of frames on time.", g_clock.RefreshHz(), g_onTargetPercent);
		else
			sprintf_s(g_status, "On: lining up with the %.2f Hz screen.", g_clock.RefreshHz());
		break;
	}
}

void Enter(State state)
{
	if (state == g_state)
		return;

	g_state = state;
	Describe(state);
	LOG("DisplaySync: %s", g_status);

	if (state == State::Locking)
		return;

	g_hasReport = false;
	Lock().Reset();
	Stats().Clear();
}

void Report()
{
	const PhaseStats& stats = Stats();

	if (stats.Samples() < kReportFrames)
		return;

	g_onTargetPercent = stats.Locked() * kPercent / stats.Samples();
	g_hasReport = true;
	Describe(State::Locking);

	LOG("DisplaySync: %d frames, %d%% within %.1f ms of the target, mean %.2f ms, worst %.2f ms, screen %.3f Hz, "
		"lead %.1f ms", stats.Samples(), g_onTargetPercent, kOnTargetWithinMs, TicksToMs(stats.MeanAbsTicks()),
		TicksToMs(stats.WorstAbsTicks()), g_clock.RefreshHz(), g_modVals.displaySyncLeadMs);

	Stats().Clear();
}

void MoveNextDeadline(int64_t waitEnd, int64_t correctionTicks)
{
	if (correctionTicks == 0)
		return;

	const int64_t moved = waitEnd + correctionTicks;
	TryWriteMemory(GameAddress(GameOffsets::kFrameWaitStart), &moved, sizeof(moved));
}

State Follow(const PhaseLock::Step& step)
{
	switch (step.verdict)
	{
	case PhaseLock::Verdict::NoClock:
		return State::NoDisplayClock;
	case PhaseLock::Verdict::RateMismatch:
		return State::RateMismatch;
	default:
		return State::Locking;
	}
}

}

void DisplaySync::OnFrame()
{
	if (!g_modVals.displaySync || Compat::SafeMode())
	{
		Enter(State::Off);
		return;
	}

	if (!LimiterIsMeasured())
	{
		Enter(State::UnknownGameCode);
		return;
	}

	Limiter limiter = {};
	if (!ReadLimiter(limiter))
	{
		Enter(State::GameClockPaused);
		return;
	}

	if (!SampleClockWhenDue())
	{
		Enter(State::NoDisplayClock);
		return;
	}

	if (!g_clock.IsOnPrimaryMonitor())
	{
		Enter(State::OtherMonitor);
		return;
	}

	Lock().SetLeadTicks(MsToTicks(g_modVals.displaySyncLeadMs));
	const PhaseLock::Step step = Lock().Update(limiter.waitEnd, limiter.framePeriodTicks, g_clock.Timing());

	Enter(Follow(step));

	if (step.verdict != PhaseLock::Verdict::Correcting)
		return;

	MoveNextDeadline(limiter.waitEnd, step.correctionTicks);
	Stats().Add(step.errorTicks);
	Report();
}

const char* DisplaySync::GetStatusText()
{
	return g_status;
}
