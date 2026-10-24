#include "D3D9/Device/ResetClock.h"

#include "Core/logger.h"
#include "D3D9/Device/ResetTimeline.h"

#include <Windows.h>

#include <mutex>

namespace {

std::mutex g_lock;
ResetTimeline g_timeline;
volatile LONG g_recovering = 0;

double NowMs()
{
	static const double ticksPerMs = []
	{
		LARGE_INTEGER frequency = {};
		QueryPerformanceFrequency(&frequency);
		return static_cast<double>(frequency.QuadPart) / 1000.0;
	}();

	LARGE_INTEGER now = {};
	QueryPerformanceCounter(&now);
	return static_cast<double>(now.QuadPart) / ticksPerMs;
}

void Report(const ResetTimeline::Summary& summary)
{
	LOG("ResetClock: back in %.0f ms (away %.0f ms): %.0f ms before the game asked for a reset, the mod's releases "
		"%.1f ms, Direct3D's Reset %.0f ms (%d refused), first frame %.0f ms after the reset",
		summary.totalMs, summary.awayMs, summary.activationToResetMs, summary.modReleaseMs, summary.direct3dMs,
		summary.failedResets, summary.resetToFrameMs);
}

}

void ResetClock::OnFocus(bool active)
{
	const double now = NowMs();
	std::lock_guard<std::mutex> hold(g_lock);

	if (active)
		g_timeline.Activated(now);
	else
		g_timeline.Deactivated(now);
}

void ResetClock::OnResetBegan()
{
	const double now = NowMs();
	std::lock_guard<std::mutex> hold(g_lock);
	g_timeline.ResetBegan(now);
}

void ResetClock::OnModReleased()
{
	const double now = NowMs();
	std::lock_guard<std::mutex> hold(g_lock);
	g_timeline.ModReleased(now);
}

void ResetClock::OnResetReturned(bool succeeded)
{
	const double now = NowMs();
	std::lock_guard<std::mutex> hold(g_lock);
	g_timeline.ResetReturned(now, succeeded);

	if (succeeded)
		InterlockedExchange(&g_recovering, 1);
}

void ResetClock::OnPresented()
{
	if (InterlockedCompareExchange(&g_recovering, 0, 0) == 0)
		return;

	const double now = NowMs();
	ResetTimeline::Summary summary = {};

	{
		std::lock_guard<std::mutex> hold(g_lock);

		if (!g_timeline.Presented(now, summary))
			return;

		InterlockedExchange(&g_recovering, 0);
	}

	Report(summary);
}
