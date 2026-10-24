#include "Core/FramePhases.h"

#include "Core/Config/interfaces.h"
#include "Core/FramePhaseStats.h"
#include "Core/SpikeReport.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/CallSiteClock.h"
#include "Hooks/CallSiteScan.h"
#include "Hooks/ImportPatch.h"

#include <Windows.h>
#include <intrin.h>

#include <atomic>
#include <cstdio>

namespace {

constexpr int kMostSites = FramePhaseStats::kMostSites;
constexpr int kMostLoopSites = 32;
constexpr int kWindowFrames = 600;
constexpr size_t kLabelBytes = 48;
constexpr double kCalibrateAfterMs = 250.0;
constexpr double kMsPerSecond = 1000.0;

struct Named
{
	uintptr_t rva;
	const char* name;
};

const Named kKnownTargets[] = {
	{ GameOffsets::kFnFrameWait, "frame wait" },
	{ GameOffsets::kFnPresent, "present" },
};

CallSiteScan::Site g_sites[kMostSites] = {};
CallSiteClock::Clock g_clocks[kMostSites] = {};
char g_labelText[kMostSites][kLabelBytes] = {};
const char* g_labels[kMostSites] = {};
uint64_t g_frameTicks[kMostSites] = {};
uint32_t g_frameCalls[kMostSites] = {};
uint8_t* g_siteCode[kMostSites] = {};
uint8_t* g_thunks = nullptr;
int g_siteCount = 0;

volatile DWORD g_gameThreadId = 0;
std::atomic<bool> g_prepared{ false };
bool g_patched = false;

FramePhaseStats::Layout g_layout = {};
FramePhaseStats::Window g_window;
SpikeReport::Limiter g_limiter;

uint64_t g_lastFrameEnd = 0;
uint64_t g_calibrateTsc = 0;
LARGE_INTEGER g_calibrateQpc = {};
LARGE_INTEGER g_frequency = {};
double g_ticksPerMs = 0.0;

uintptr_t Rva(uintptr_t address)
{
	return address - GetGameBaseAddress();
}

const char* KnownName(uintptr_t target)
{
	for (const Named& known : kKnownTargets)
	{
		if (CodeSignatures::Address(known.rva) == target)
			return known.name;
	}

	return nullptr;
}

void NameTarget(const CallSiteScan::Site& site, char* out, size_t size)
{
	if (site.kind == CallSiteScan::Kind_Import)
	{
		if (!ImportPatch::NameOfSlot(GetModuleHandleA(nullptr), site.operand, out, size))
			sprintf_s(out, size, "[%x]", static_cast<unsigned>(Rva(site.operand)));
		return;
	}

	const char* const known = KnownName(site.operand);

	if (known != nullptr)
	{
		sprintf_s(out, size, "%s", known);
		return;
	}

	sprintf_s(out, size, "%x", static_cast<unsigned>(Rva(site.operand)));
}

void NameSite(int index, uintptr_t siteAddress)
{
	char target[kLabelBytes] = {};
	NameTarget(g_sites[index], target, sizeof(target));

	sprintf_s(g_labelText[index], "%s@%x", target, static_cast<unsigned>(Rva(siteAddress)));
	g_labels[index] = g_labelText[index];
}

bool AddSite(const CallSiteScan::Site& site, uint8_t* function)
{
	if (g_siteCount >= kMostSites)
		return false;

	g_sites[g_siteCount] = site;
	g_siteCode[g_siteCount] = function + site.offset;
	NameSite(g_siteCount, reinterpret_cast<uintptr_t>(function + site.offset));
	++g_siteCount;
	return true;
}

int Scan(uint8_t* function, CallSiteScan::Site* out, int capacity)
{
	if (function == nullptr || !IsAddressInGameModule(reinterpret_cast<uintptr_t>(function)))
		return -1;

	return CallSiteScan::Find(function, reinterpret_cast<uintptr_t>(function), out, capacity);
}

bool AddBodySite(uint8_t* loop, uintptr_t runFrame)
{
	CallSiteScan::Site found[kMostLoopSites] = {};
	const int count = Scan(loop, found, kMostLoopSites);

	for (int i = 0; i < count; ++i)
	{
		if (found[i].kind != CallSiteScan::Kind_Direct || found[i].operand != runFrame)
			continue;

		g_layout.bodySite = g_siteCount;
		return AddSite(found[i], loop);
	}

	return false;
}

void AddFrameSites(uint8_t* runFrame)
{
	CallSiteScan::Site found[kMostSites] = {};
	const int count = Scan(runFrame, found, kMostSites - g_siteCount);
	const uintptr_t frameWait = CodeSignatures::Address(GameOffsets::kFnFrameWait);

	for (int i = 0; i < count; ++i)
	{
		if (found[i].kind == CallSiteScan::Kind_Direct && found[i].operand == frameWait)
			g_layout.waitSite = g_siteCount;

		AddSite(found[i], runFrame);
	}
}

void Calibrate(uint64_t now)
{
	LARGE_INTEGER qpc;
	QueryPerformanceCounter(&qpc);

	const double elapsedMs = static_cast<double>(qpc.QuadPart - g_calibrateQpc.QuadPart) * kMsPerSecond /
		static_cast<double>(g_frequency.QuadPart);

	if (elapsedMs < kCalibrateAfterMs)
		return;

	g_ticksPerMs = static_cast<double>(now - g_calibrateTsc) / elapsedMs;
}

FramePhaseStats::Frame TakeFrame(uint64_t intervalTicks)
{
	for (int i = 0; i < g_siteCount; ++i)
	{
		g_frameTicks[i] = g_clocks[i].ticks;
		g_frameCalls[i] = g_clocks[i].calls;
		g_clocks[i].ticks = 0;
		g_clocks[i].calls = 0;
	}

	return { g_frameTicks, g_frameCalls, intervalTicks };
}

bool IsSlow(uint64_t intervalTicks)
{
	return g_modVals.framePhasesSlowMs > 0 &&
		static_cast<double>(intervalTicks) >= g_modVals.framePhasesSlowMs * g_ticksPerMs;
}

void ReportWindow()
{
	LOG("%s", g_window.Describe(g_labels, g_ticksPerMs).c_str());
	g_window.Reset(g_layout);
}

void __cdecl OnFrameEnd()
{
	const uint64_t now = __rdtsc();
	const uint64_t last = g_lastFrameEnd;
	g_lastFrameEnd = now;

	Calibrate(now);

	const FramePhaseStats::Frame frame = TakeFrame(now - last);

	if (last == 0 || g_ticksPerMs <= 0.0)
		return;

	g_window.Add(frame, g_ticksPerMs);

	if (IsSlow(frame.intervalTicks) && g_limiter.Allow(GetTickCount()))
		LOG("%s", FramePhaseStats::DescribeFrame(frame, g_layout, g_labels, g_ticksPerMs).c_str());

	if (g_window.Frames() >= kWindowFrames)
		ReportWindow();
}

bool WriteThunks()
{
	g_thunks = static_cast<uint8_t*>(VirtualAlloc(nullptr, g_siteCount * CallSiteClock::kThunkBytes,
		MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

	if (g_thunks == nullptr)
		return false;

	for (int i = 0; i < g_siteCount; ++i)
	{
		const CallSiteClock::LeaveCallback onLeave = i == g_layout.bodySite ? &OnFrameEnd : nullptr;

		if (CallSiteClock::Write(g_thunks + i * CallSiteClock::kThunkBytes, &g_clocks[i], &g_gameThreadId,
			g_sites[i], onLeave) == 0)
			return false;
	}

	return true;
}

int Redirect()
{
	int redirected = 0;

	for (int i = 0; i < g_siteCount; ++i)
	{
		if (CallSiteClock::Redirect(g_siteCode[i], g_sites[i], g_thunks + i * CallSiteClock::kThunkBytes))
			++redirected;
	}

	return redirected;
}

}

bool FramePhases::Prepare()
{
	if (!g_modVals.framePhases)
		return false;

	g_layout.bodySite = -1;
	g_layout.waitSite = -1;

	uint8_t* const loop = reinterpret_cast<uint8_t*>(CodeSignatures::Address(GameOffsets::kFnGameThread));
	const uintptr_t runFrame = CodeSignatures::Address(GameOffsets::kFnRunFrame);

	if (!AddBodySite(loop, runFrame))
	{
		LOG("FramePhases: the game thread's call to the frame was not found, nothing is timed");
		return false;
	}

	AddFrameSites(reinterpret_cast<uint8_t*>(runFrame));
	g_layout.siteCount = g_siteCount;

	if (g_siteCount <= 1 || !WriteThunks())
	{
		LOG("FramePhases: the frame's calls could not be prepared (%d found), nothing is timed", g_siteCount - 1);
		return false;
	}

	g_window.Reset(g_layout);
	g_prepared = true;
	LOG("FramePhases: %d calls inside the frame are ready to be timed, frame wait %s", g_siteCount - 1,
		g_layout.waitSite >= 0 ? "found" : "not found");
	return true;
}

void FramePhases::OnPresent()
{
	if (!g_prepared || g_patched)
		return;

	g_patched = true;
	QueryPerformanceFrequency(&g_frequency);
	QueryPerformanceCounter(&g_calibrateQpc);
	g_calibrateTsc = __rdtsc();
	g_gameThreadId = GetCurrentThreadId();

	const int redirected = Redirect();
	LOG("FramePhases: timing %d of %d calls on thread %lu", redirected, g_siteCount, g_gameThreadId);
}
