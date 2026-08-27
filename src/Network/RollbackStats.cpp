#include "Network/RollbackStats.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Network/SteamNetwork.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

struct GgpoNetworkStats
{
	int sendQueueLen;
	int recvQueueLen;
	int ping;
	int kbpsSent;
	int localFramesBehind;
	int remoteFramesBehind;
};

using GetNetworkStats_t = int(__fastcall*)(void*, void*, GgpoNetworkStats*, int);

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

void* ResolveSession()
{
	uint32_t pointer = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kGgpoSession)),
		pointer))
	{
		return nullptr;
	}

	if (pointer == 0)
		return nullptr;

	void* session = reinterpret_cast<void*>(pointer);
	if (!IsReadableMemory(session, sizeof(void*)))
		return nullptr;

	uint32_t vtable = 0;
	if (!TryReadDword(session, vtable))
		return nullptr;

	if (vtable != RvaToAddress(GameOffsets::kGgpoBackendVTable))
		return nullptr;

	return session;
}

bool QueryNetworkStats(GgpoNetworkStats& out)
{
	memset(&out, 0, sizeof(out));

	if (!g_modVals.netplayDiagnostics)
		return false;

	if (!SteamNetwork::HasRecentPeerTraffic(3000))
		return false;

	void* session = ResolveSession();
	if (session == nullptr)
		return false;

	void** vtable = *reinterpret_cast<void***>(session);
	if (!IsReadableMemory(vtable, sizeof(void*) * (GameOffsets::kGgpoGetNetworkStats + 1)))
		return false;

	auto getStats = reinterpret_cast<GetNetworkStats_t>(vtable[GameOffsets::kGgpoGetNetworkStats]);
	if (getStats == nullptr)
		return false;

	bool got = false;

	for (int handle = 1; handle <= 2; ++handle)
	{
		GgpoNetworkStats stats = {};

		__try
		{
			if (getStats(session, nullptr, &stats, handle) != 0)
				continue;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return got;
		}

		if (stats.ping <= 0 && got)
			continue;

		out = stats;
		got = true;
	}

	return got;
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
	QueryPerformanceCounter(&g_rateAnchor);
	LOG("RollbackStats: netplay started, capturing the first %d frames",
		RollbackStats::kStartSamples);
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

	GgpoNetworkStats stats = {};
	if (QueryNetworkStats(stats))
	{
		sample.ping = stats.ping;
		sample.localFramesBehind = stats.localFramesBehind;
		sample.remoteFramesBehind = stats.remoteFramesBehind;
		sample.sendQueue = stats.sendQueueLen;
		sample.kbpsSent = stats.kbpsSent;

		sprintf_s(g_status, "frame %d, %d rollbacks, %d ms", sample.frame, sample.rollbacks,
			sample.ping);
	}
	else
	{
		sprintf_s(g_status, "frame %d, %d rollbacks, no GGPO stats", sample.frame,
			sample.rollbacks);
	}

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
	return ResolveSession() != nullptr;
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
