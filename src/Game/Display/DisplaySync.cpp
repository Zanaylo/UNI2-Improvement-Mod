#include "Game/Display/DisplaySync.h"

#include "Core/Boot/Compat.h"
#include "Core/CodeFingerprint.h"
#include "Core/Config/interfaces.h"
#include "Core/MeasuredCode.h"
#include "Core/SampleTotals.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Display/DelayBias.h"
#include "Game/Display/PhaseStats.h"
#include "Game/Display/PresentPlanner.h"
#include "Game/Display/ScanlineClock.h"
#include "Game/Engine/GameOffsets.h"

#include <Windows.h>

#include <cmath>
#include <cstdio>

namespace {

constexpr double kMaxRateMismatch = 0.002;
constexpr double kOnTargetWithinMs = 0.5;
constexpr double kSlowestScanlineReadMs = 0.05;
constexpr double kAnchorRetryEveryMs = 0.25;
constexpr double kAnchorRetryStopsBeforeTargetMs = 2.0;
constexpr double kBiasSafetyMs = 1.0;
constexpr double kMillisecondsPerSecond = 1000.0;
constexpr int kWorkFramesPerBucket = 60;
constexpr int kMonitorCheckFrames = 60;
constexpr int kReopenAfterFailureFrames = 600;
constexpr int kReportFrames = 600;
constexpr int kPercent = 100;

const CodeFingerprint kLimiterCode[] = {
	{ GameOffsets::kSiteFrameWaitHasQpcRead, { 0x83, 0x3D }, 2, GameOffsets::kFrameWaitHasQpc, { 0x00 }, 1 },
	{ GameOffsets::kSiteFrameWaitFrozenRead, { 0x8B, 0x0D }, 2, GameOffsets::kFrameWaitFrozenNow, {}, 0 },
	{ GameOffsets::kSiteFrameWaitPeriodRead, { 0xF3, 0x0F, 0x10, 0x15 }, 4, GameOffsets::kFramePeriodSeconds, {}, 0 },
	{ GameOffsets::kSiteFrameWaitPassCallbackRead, { 0xA1 }, 1, GameOffsets::kFrameWaitPassCallback,
		{ 0x85, 0xC0, 0x74, 0x02, 0xFF, 0xD0 }, 6 },
	{ GameOffsets::kSiteFrameWaitLoopStartRead, { 0x2B, 0x0D }, 2, GameOffsets::kFrameWaitStart, {}, 0 },
	{ GameOffsets::kSiteFrameWaitStartWrite, { 0x89, 0x0D }, 2, GameOffsets::kFrameWaitStart, {}, 0 },
};

using WaitPassCallback = void(__cdecl*)();

enum class State
{
	Off,
	UnknownGameCode,
	GameClockPaused,
	NoDisplayClock,
	RateMismatch,
	Locking,
};

enum class Readiness
{
	Unchecked,
	Ready,
	Refused,
};

struct GameLimiter
{
	int64_t* windowStart;
	const int64_t* frozenNow;
	const uint32_t* hasQpc;
	const float* periodSeconds;
	WaitPassCallback* passCallback;
};

struct FramePlan
{
	bool planned;
	PresentPlanner::Plan plan;
	int64_t plannedAt;
};

Readiness g_readiness = Readiness::Unchecked;
State g_state = State::Off;
GameLimiter g_game = {};
bool g_armed = false;
bool g_needsAnchor = false;
bool g_clockFailed = false;
int64_t g_nextAnchorTry = 0;
int64_t g_framePeriodTicks = 0;
int64_t g_plannedWindowStart = 0;
FramePlan g_framePlan = {};
int64_t g_lastWaitEnd = 0;
bool g_hasLastWaitEnd = false;
bool g_waitEndedThisFrame = false;
int64_t g_releasedAt = 0;
int64_t g_nextTarget = 0;
bool g_hasNextTarget = false;
int g_framesUntilMonitorCheck = 0;
int g_onTargetPercent = 0;
double g_givenBiasMs = 0.0;
bool g_hasReport = false;
SampleTotals g_modWorkMs;
SampleTotals g_biasMs;
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

int64_t Now()
{
	LARGE_INTEGER value = {};
	QueryPerformanceCounter(&value);
	return value.QuadPart;
}

int64_t MsToTicks(double milliseconds)
{
	return static_cast<int64_t>(std::llround(milliseconds * Frequency() / kMillisecondsPerSecond));
}

double TicksToMs(int64_t ticks)
{
	return static_cast<double>(ticks) * kMillisecondsPerSecond / Frequency();
}

ScanlineClock& Clock()
{
	static ScanlineClock clock(Frequency(), MsToTicks(kSlowestScanlineReadMs));
	return clock;
}

PresentPlanner& Planner()
{
	static PresentPlanner planner(kMaxRateMismatch);
	return planner;
}

PhaseStats& Stats()
{
	static PhaseStats stats(MsToTicks(kOnTargetWithinMs));
	return stats;
}

DelayBias& Bias()
{
	static DelayBias bias(MsToTicks(kBiasSafetyMs), kWorkFramesPerBucket);
	return bias;
}

template <typename T>
T* GameAddress(uintptr_t rva)
{
	return reinterpret_cast<T*>(RvaToAddress(rva));
}

void TakeAnchorWhenDue(int64_t now)
{
	if (!g_needsAnchor || now < g_nextAnchorTry)
		return;

	if (g_framePlan.planned && now > g_framePlan.plan.targetTicks - MsToTicks(kAnchorRetryStopsBeforeTargetMs))
		return;

	const ScanlineClock::Reading reading = Clock().Sample();
	g_needsAnchor = reading == ScanlineClock::Reading::Unusable;
	g_clockFailed = g_clockFailed || reading == ScanlineClock::Reading::Failed;
	g_nextAnchorTry = now + MsToTicks(kAnchorRetryEveryMs);
}

bool IsNewWait()
{
	return *g_game.windowStart != g_plannedWindowStart && *g_game.frozenNow == 0 && *g_game.hasQpc != 0;
}

void PlanThisFrame()
{
	g_needsAnchor = true;
	g_nextAnchorTry = 0;
	TakeAnchorWhenDue(Now());

	const int64_t now = Now();
	const int64_t deadline = *g_game.windowStart + g_framePeriodTicks;
	const PresentPlanner::Plan plan = Planner().Next(now, deadline, g_framePeriodTicks, Clock().Grid());

	if (plan.verdict == PresentPlanner::Verdict::OnSlot || plan.verdict == PresentPlanner::Verdict::Late)
		*g_game.windowStart = plan.targetTicks - g_framePeriodTicks;

	g_plannedWindowStart = *g_game.windowStart;
	g_framePlan = { true, plan, now };
}

void __cdecl OnWaitPass()
{
	if (!g_armed)
		return;

	if (IsNewWait())
	{
		PlanThisFrame();
		return;
	}

	if (g_needsAnchor)
		TakeAnchorWhenDue(Now());
}

bool LimiterIsMeasured()
{
	if (g_readiness != Readiness::Unchecked)
		return g_readiness == Readiness::Ready;

	const int mismatch = MeasuredCode::FirstMismatch(kLimiterCode);
	g_readiness = mismatch == MeasuredCode::kAllMatch ? Readiness::Ready : Readiness::Refused;

	if (g_readiness == Readiness::Refused)
	{
		LOG("DisplaySync: rva 0x%x is not the measured 1.40 code, frames keep the game's own timing",
			static_cast<unsigned>(kLimiterCode[mismatch].siteRva));

		return false;
	}

	g_game = {
		GameAddress<int64_t>(GameOffsets::kFrameWaitStart),
		GameAddress<const int64_t>(GameOffsets::kFrameWaitFrozenNow),
		GameAddress<const uint32_t>(GameOffsets::kFrameWaitHasQpc),
		GameAddress<const float>(GameOffsets::kFramePeriodSeconds),
		GameAddress<WaitPassCallback>(GameOffsets::kFrameWaitPassCallback),
	};

	if (*g_game.passCallback != nullptr)
	{
		g_readiness = Readiness::Refused;
		LOG("DisplaySync: the frame wait already calls 0x%p on every pass, frames keep the game's own timing",
			reinterpret_cast<void*>(*g_game.passCallback));

		return false;
	}

	LOG("DisplaySync: the frame limiter matches the measured 1.40 code");
	return true;
}

bool ReadFramePeriod()
{
	if (*g_game.hasQpc == 0 || *g_game.frozenNow != 0 || *g_game.periodSeconds <= 0.0f)
		return false;

	g_framePeriodTicks = static_cast<int64_t>(std::llround(*g_game.periodSeconds * Frequency()));
	return true;
}

HWND GameWindow()
{
	uint32_t handle = 0;
	TryReadDword(GameAddress<const void>(GameOffsets::kWindowHandle), handle);
	return reinterpret_cast<HWND>(static_cast<uintptr_t>(handle));
}

bool ClockIsOpen()
{
	if (--g_framesUntilMonitorCheck > 0 && !g_clockFailed)
		return Clock().HasAnchor();

	g_framesUntilMonitorCheck = kMonitorCheckFrames;

	const HWND window = GameWindow();
	const HMONITOR monitor = window != nullptr ? MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) : nullptr;

	if (Clock().IsOpenOn(monitor) && !g_clockFailed)
		return Clock().HasAnchor();

	g_clockFailed = false;

	if (!Clock().Open(window))
	{
		g_framesUntilMonitorCheck = kReopenAfterFailureFrames;
		return false;
	}

	LOG("DisplaySync: reading the scanline of a %.3f Hz screen", Clock().RefreshHz());
	return Clock().HasAnchor();
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
		sprintf_s(g_status, "Windows did not report where the screen is in its refresh.");
		break;
	case State::RateMismatch:
		sprintf_s(g_status, "Your screen runs at %.2f Hz, which cannot show 60 frames evenly.", Clock().RefreshHz());
		break;
	case State::Locking:
		if (!g_hasReport)
			sprintf_s(g_status, "On: lining up with the %.2f Hz screen.", Clock().RefreshHz());
		else if (g_givenBiasMs > 0.0)
			sprintf_s(g_status, "On: %.2f Hz screen, %d%% of frames on time, input read %.1f ms later.",
				Clock().RefreshHz(), g_onTargetPercent, g_givenBiasMs);
		else
			sprintf_s(g_status, "On: %.2f Hz screen, %d%% of frames on time.", Clock().RefreshHz(), g_onTargetPercent);
		break;
	}
}

void ForgetFrames()
{
	g_hasReport = false;
	g_hasLastWaitEnd = false;
	g_waitEndedThisFrame = false;
	g_hasNextTarget = false;
	g_framePlan = {};
	Stats().Clear();
	Bias().Clear();
	g_modWorkMs.Clear();
	g_biasMs.Clear();
}

void Enter(State state)
{
	if (state == g_state)
		return;

	const bool wasLocking = g_state == State::Locking;
	g_state = state;
	Describe(state);
	LOG("DisplaySync: %s", g_status);

	if (wasLocking)
		ForgetFrames();
}

void RemoveCallback()
{
	if (g_game.passCallback == nullptr || *g_game.passCallback != &OnWaitPass)
		return;

	*g_game.passCallback = nullptr;
	LOG("DisplaySync: the frame wait no longer calls the mod");
}

void InstallCallback()
{
	if (*g_game.passCallback == &OnWaitPass)
		return;

	*g_game.passCallback = &OnWaitPass;
	LOG("DisplaySync: the frame wait now calls the mod on every pass");
}

void Stop(State state)
{
	g_armed = false;
	g_releasedAt = 0;
	Enter(state);

	if (state == State::Off)
		RemoveCallback();
}

State Follow(PresentPlanner::Verdict verdict)
{
	switch (verdict)
	{
	case PresentPlanner::Verdict::NoClock:
		return State::NoDisplayClock;
	case PresentPlanner::Verdict::RateMismatch:
		return State::RateMismatch;
	default:
		return State::Locking;
	}
}

void Report()
{
	const PhaseStats& stats = Stats();

	if (stats.Frames() < kReportFrames)
		return;

	g_onTargetPercent = stats.Locked() * kPercent / stats.Frames();
	g_givenBiasMs = g_biasMs.Mean();
	g_hasReport = true;
	Describe(State::Locking);

	LOG("DisplaySync: %d frames, %d late, %d%% within %.1f ms of the slot (worst %.2f ms), game work worst %.2f ms, "
		"mod work mean %.2f worst %.2f ms, input bias asked %.1f given %.2f ms, screen %.3f Hz, lead %.1f ms",
		stats.Frames(), stats.Late(), g_onTargetPercent, kOnTargetWithinMs, TicksToMs(stats.WorstAbsTicks()),
		TicksToMs(Bias().WorstWorkTicks()), g_modWorkMs.Mean(), g_modWorkMs.Max(), g_modVals.displaySyncDelayBiasMs,
		g_givenBiasMs, Clock().RefreshHz(), g_modVals.displaySyncLeadMs);

	Stats().Clear();
	g_modWorkMs.Clear();
	g_biasMs.Clear();
}

void AddWork(int64_t workStartedTicks)
{
	if (g_releasedAt == 0)
		return;

	Bias().AddWork(workStartedTicks - g_releasedAt);
}

void CountLateFrame()
{
	if (g_state != State::Locking)
		return;

	AddWork(Now());
	Stats().AddLate();
	Report();
}

void CountPlannedFrame(const FramePlan& frame, int64_t waitEnd)
{
	Enter(Follow(frame.plan.verdict));

	if (g_state != State::Locking)
		return;

	AddWork(frame.plannedAt);

	if (frame.plan.verdict == PresentPlanner::Verdict::Late)
	{
		Stats().AddLate();
		Report();
		return;
	}

	g_nextTarget = frame.plan.targetTicks + frame.plan.refreshesPerFrame * Clock().Grid().refreshTicks;
	g_hasNextTarget = true;
	Stats().Add(waitEnd - frame.plan.targetTicks);
	Report();
}

void CountFrame()
{
	const FramePlan frame = g_framePlan;
	g_framePlan.planned = false;
	g_hasNextTarget = false;

	const int64_t waitEnd = *g_game.windowStart;
	g_waitEndedThisFrame = !g_hasLastWaitEnd || waitEnd != g_lastWaitEnd;

	if (!g_waitEndedThisFrame)
		return;

	const bool hadPreviousWait = g_hasLastWaitEnd;
	g_lastWaitEnd = waitEnd;
	g_hasLastWaitEnd = true;

	if (frame.planned)
	{
		CountPlannedFrame(frame, waitEnd);
		return;
	}

	if (hadPreviousWait)
		CountLateFrame();
}

int64_t SpinUntil(int64_t deadline)
{
	int64_t now = Now();

	while (now < deadline)
	{
		YieldProcessor();
		now = Now();
	}

	return now;
}

}

void DisplaySync::OnFrame()
{
	if (!g_modVals.displaySync || Compat::SafeMode())
	{
		Stop(State::Off);
		return;
	}

	if (!LimiterIsMeasured())
	{
		Stop(State::UnknownGameCode);
		return;
	}

	if (!ReadFramePeriod())
	{
		Stop(State::GameClockPaused);
		return;
	}

	if (!ClockIsOpen())
	{
		Stop(State::NoDisplayClock);
		return;
	}

	Planner().SetLeadTicks(MsToTicks(g_modVals.displaySyncLeadMs));
	InstallCallback();
	CountFrame();
	g_armed = true;
}

void DisplaySync::OnPresenting()
{
	if (g_state != State::Locking || !g_waitEndedThisFrame)
		return;

	g_modWorkMs.Add(TicksToMs(Now() - g_lastWaitEnd));
}

void DisplaySync::OnPresented()
{
	const int64_t now = Now();

	if (g_state != State::Locking || !g_hasNextTarget)
	{
		g_releasedAt = now;
		return;
	}

	const int64_t bias = Bias().Allowed(MsToTicks(g_modVals.displaySyncDelayBiasMs), g_nextTarget - now);
	g_biasMs.Add(TicksToMs(bias));
	g_releasedAt = SpinUntil(now + bias);
}

const char* DisplaySync::GetStatusText()
{
	return g_status;
}
