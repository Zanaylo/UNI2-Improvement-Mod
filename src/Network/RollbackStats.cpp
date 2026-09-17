#include "Network/RollbackStats.h"

#include "Core/Profiler.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Network/NetLink.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

RollbackStats::Sample g_live[RollbackStats::kLiveSamples] = {};
int g_liveCount = 0;
int g_liveNext = 0;

RollbackStats::Sample g_start[RollbackStats::kStartSamples] = {};
int g_startCount = 0;
bool g_startArmed = false;

RollbackStats::Sample g_latest = {};

bool g_active = false;
bool g_wasActive = false;

LARGE_INTEGER g_frequency = {};
LARGE_INTEGER g_lastTick = {};
LARGE_INTEGER g_rateAnchor = {};
int g_rateAnchorRollbacks = 0;
float g_rollbacksPerSecond = 0.0f;

char g_status[128] = "no netplay";

constexpr float kSlowFrameMs = 20.0f;
constexpr int kSummaryBytes = 1024;

bool g_profilerLent = false;
int g_slowFrames = 0;
float g_worstFrameMs = 0.0f;
float g_peakRollbacksPerSecond = 0.0f;

bool ReadByteAt(uintptr_t rva, uint8_t& out)
{
	return TryReadMemory(&out, reinterpret_cast<const void*>(RvaToAddress(rva)), sizeof(out));
}

bool ReadIntAt(uintptr_t rva, int& out)
{
	uint32_t value = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value))
		return false;

	out = static_cast<int>(value);
	return true;
}

void PushLive(const RollbackStats::Sample& sample)
{
	g_live[g_liveNext] = sample;
	g_liveNext = (g_liveNext + 1) % RollbackStats::kLiveSamples;

	if (g_liveCount < RollbackStats::kLiveSamples)
		++g_liveCount;
}

void PushStart(const RollbackStats::Sample& sample)
{
	if (!g_startArmed || g_startCount >= RollbackStats::kStartSamples)
		return;

	g_start[g_startCount] = sample;
	++g_startCount;
}

float ElapsedSeconds(const LARGE_INTEGER& from, const LARGE_INTEGER& to)
{
	if (g_frequency.QuadPart == 0)
		return 0.0f;

	return static_cast<float>(
		static_cast<double>(to.QuadPart - from.QuadPart) /
		static_cast<double>(g_frequency.QuadPart));
}

void OnNetplayStarted()
{
	g_startCount = 0;
	g_startArmed = true;
	g_rateAnchorRollbacks = 0;
	g_slowFrames = 0;
	g_worstFrameMs = 0.0f;
	g_peakRollbacksPerSecond = 0.0f;
	QueryPerformanceCounter(&g_rateAnchor);

	g_profilerLent = !Profiler::IsEnabled();

	if (g_profilerLent)
		Profiler::SetEnabled(true);

	LOG("RollbackStats: netplay started, capturing the first %d frames",
		RollbackStats::kStartSamples);
}

void OnNetplayEnded()
{
	char summary[kSummaryBytes] = {};
	Profiler::BuildSummary(summary, sizeof(summary));

	LOG("RollbackStats: netplay ended after %d frames and %d rollbacks, peak %.1f rollbacks per "
		"second, %d frame(s) over %.0f ms, worst %.1f ms, last ping %d ms", g_latest.frame,
		g_latest.rollbacks, g_peakRollbacksPerSecond, g_slowFrames, kSlowFrameMs, g_worstFrameMs,
		g_latest.ping);

	LOG_RAW("%s", summary);
	Profiler::DumpToLog();

	if (!g_profilerLent)
		return;

	g_profilerLent = false;
	Profiler::SetEnabled(false);
}

void Track(const RollbackStats::Sample& sample)
{
	if (sample.frameMs > kSlowFrameMs)
		++g_slowFrames;

	if (sample.frameMs > g_worstFrameMs)
		g_worstFrameMs = sample.frameMs;

	if (sample.rollbacksPerSecond > g_peakRollbacksPerSecond)
		g_peakRollbacksPerSecond = sample.rollbacksPerSecond;
}

}

void RollbackStats::Update()
{
	if (g_frequency.QuadPart == 0)
		QueryPerformanceFrequency(&g_frequency);

	LARGE_INTEGER now = {};
	QueryPerformanceCounter(&now);

	uint8_t active = 0;
	g_active = ReadByteAt(GameOffsets::kNetplayActive, active) && active != 0;

	if (g_active && !g_wasActive)
		OnNetplayStarted();

	if (!g_active && g_wasActive)
		OnNetplayEnded();

	g_wasActive = g_active;

	if (!g_active)
	{
		strncpy_s(g_status, "no netplay", _TRUNCATE);
		g_rollbacksPerSecond = 0.0f;
		g_lastTick = now;
		return;
	}

	Sample sample = {};

	ReadIntAt(GameOffsets::kNetplayFrame, sample.frame);
	ReadIntAt(GameOffsets::kRollbackCount, sample.rollbacks);

	const float sinceAnchor = ElapsedSeconds(g_rateAnchor, now);
	if (sinceAnchor >= 0.5f)
	{
		g_rollbacksPerSecond =
			static_cast<float>(sample.rollbacks - g_rateAnchorRollbacks) / sinceAnchor;
		g_rateAnchor = now;
		g_rateAnchorRollbacks = sample.rollbacks;
	}

	sample.rollbacksPerSecond = g_rollbacksPerSecond;

	if (g_lastTick.QuadPart != 0)
		sample.frameMs = ElapsedSeconds(g_lastTick, now) * 1000.0f;

	g_lastTick = now;

	const NetLink::Snapshot& link = NetLink::Current();

	if (link.hasPeer)
	{
		sample.ping = link.peer.ping;
		sample.localFramesBehind = link.peer.localBehind;
		sample.remoteFramesBehind = link.peer.remoteBehind;
		sample.sendQueue = link.peer.pending;
		sample.kbpsSent = link.peer.kbps;

		sprintf_s(g_status, "frame %d, %d rollbacks, %d ms", sample.frame, sample.rollbacks, sample.ping);
	}
	else
	{
		sprintf_s(g_status, "frame %d, %d rollbacks, no peer", sample.frame, sample.rollbacks);
	}

	Track(sample);

	g_latest = sample;
	PushLive(sample);
	PushStart(sample);
}

bool RollbackStats::IsNetplayActive()
{
	return g_active;
}

bool RollbackStats::HasSession()
{
	return NetLink::Current().backend == NetLink::Backend_Players;
}

int RollbackStats::GetFrame()
{
	return g_latest.frame;
}

int RollbackStats::GetRollbackTotal()
{
	return g_latest.rollbacks;
}

float RollbackStats::GetRollbacksPerSecond()
{
	return g_rollbacksPerSecond;
}

const RollbackStats::Sample& RollbackStats::GetLatest()
{
	return g_latest;
}

int RollbackStats::LiveCount()
{
	return g_liveCount;
}

const RollbackStats::Sample& RollbackStats::Live(int index)
{
	if (index < 0 || index >= g_liveCount)
		return g_latest;

	const int oldest = (g_liveNext - g_liveCount + kLiveSamples) % kLiveSamples;
	return g_live[(oldest + index) % kLiveSamples];
}

int RollbackStats::StartCount()
{
	return g_startCount;
}

const RollbackStats::Sample& RollbackStats::Start(int index)
{
	if (index < 0 || index >= g_startCount)
		return g_latest;

	return g_start[index];
}

bool RollbackStats::StartCaptureComplete()
{
	return g_startCount >= kStartSamples;
}

void RollbackStats::ClearStartCapture()
{
	g_startCount = 0;
	g_startArmed = true;
}

const char* RollbackStats::GetStatusText()
{
	return g_status;
}
