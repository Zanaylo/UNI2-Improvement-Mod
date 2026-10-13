#include "Core/StallSampler.h"

#include "Core/StallProfile.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Hooks/HookManager.h"

#include <Windows.h>
#include <psapi.h>

#include <cstdint>

namespace {

constexpr double kArmAfterMs = 20.0;
constexpr LONGLONG kPeriod100ns = -10000;
constexpr size_t kStackWords = 4096;
constexpr int kReportedSites = 5;

struct ModuleRange
{
	uintptr_t start;
	uintptr_t end;
	StallProfile::Leaf leaf;
};

constexpr int kMostRanges = 10;

const struct
{
	const char* name;
	StallProfile::Leaf leaf;
} kKnownModules[] = {
	{ "ntdll.dll", StallProfile::Leaf_Kernel },
	{ "kernelbase.dll", StallProfile::Leaf_Kernel },
	{ "kernel32.dll", StallProfile::Leaf_Kernel },
	{ "user32.dll", StallProfile::Leaf_User },
	{ "win32u.dll", StallProfile::Leaf_User },
	{ "d3d9.dll", StallProfile::Leaf_Direct3D },
	{ "d3dx9_42.dll", StallProfile::Leaf_Direct3D },
};

ModuleRange g_ranges[kMostRanges] = {};
int g_rangeCount = 0;

StallProfile::Image g_image = {};
StallProfile::Table g_table;
uint32_t g_stack[kStackWords] = {};

LARGE_INTEGER g_frequency = {};
volatile LONGLONG g_frameStart = 0;
volatile LONG g_harvesting = 0;
volatile LONG g_sampling = 0;
volatile LONG g_running = 0;

HANDLE g_target = nullptr;
DWORD g_targetId = 0;
uintptr_t g_targetStackBase = 0;

void AddRange(HMODULE module, StallProfile::Leaf leaf)
{
	MODULEINFO info = {};

	if (module == nullptr || g_rangeCount >= kMostRanges ||
		!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
		return;

	const uintptr_t start = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
	g_ranges[g_rangeCount++] = { start, start + info.SizeOfImage, leaf };
}

void LearnModules()
{
	AddRange(GetModuleHandleA(nullptr), StallProfile::Leaf_Game);
	AddRange(GetModModuleHandle(), StallProfile::Leaf_Mod);

	for (const auto& known : kKnownModules)
		AddRange(GetModuleHandleA(known.name), known.leaf);
}

StallProfile::Leaf LeafOf(uintptr_t instruction)
{
	for (int i = 0; i < g_rangeCount; ++i)
	{
		if (instruction >= g_ranges[i].start && instruction < g_ranges[i].end)
			return g_ranges[i].leaf;
	}

	return StallProfile::Leaf_Other;
}

bool LearnImage()
{
	uintptr_t start = 0;
	size_t size = 0;

	if (!HookManager::GetSectionBounds(".text", start, size))
		return false;

	g_image.base = static_cast<uint32_t>(GetGameBaseAddress());
	g_image.codeStart = static_cast<uint32_t>(start);
	g_image.codeSize = static_cast<uint32_t>(size);
	g_image.code = reinterpret_cast<const uint8_t*>(start);

	return g_image.base != 0;
}

LONGLONG Now()
{
	LARGE_INTEGER now = {};
	QueryPerformanceCounter(&now);

	return now.QuadPart;
}

bool FrameIsLong()
{
	const LONGLONG start = g_frameStart;

	if (start == 0)
		return false;

	return static_cast<double>(Now() - start) * 1000.0 / static_cast<double>(g_frequency.QuadPart) >= kArmAfterMs;
}

size_t CopyStack(uintptr_t top)
{
	if (top == 0 || top >= g_targetStackBase)
		return 0;

	const size_t live = (g_targetStackBase - top) / sizeof(uint32_t);
	const size_t words = live < kStackWords ? live : kStackWords;

	return TryReadMemory(g_stack, reinterpret_cast<const void*>(top), words * sizeof(uint32_t)) ? words : 0;
}

void SampleTarget()
{
	CONTEXT context = {};
	context.ContextFlags = CONTEXT_CONTROL;

	if (SuspendThread(g_target) == static_cast<DWORD>(-1))
		return;

	const bool read = GetThreadContext(g_target, &context) != FALSE;
	const size_t words = read ? CopyStack(context.Esp) : 0;

	ResumeThread(g_target);

	if (!read)
		return;

	uint32_t chain[StallProfile::kChainDepth] = {};
	const int depth = StallProfile::BuildChain(g_image, context.Eip, g_stack, words, chain);

	g_table.Add(chain, depth, LeafOf(context.Eip));
}

void SampleOnce()
{
	InterlockedExchange(&g_sampling, 1);

	if (InterlockedCompareExchange(&g_harvesting, 0, 0) == 0 && g_target != nullptr && FrameIsLong())
		SampleTarget();

	InterlockedExchange(&g_sampling, 0);
}

DWORD WINAPI SamplerThread(LPVOID)
{
	const HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
		TIMER_ALL_ACCESS);

	if (timer == nullptr)
	{
		LOG("StallSampler: no high resolution timer, nothing is sampled");
		return 0;
	}

	LARGE_INTEGER due = {};
	due.QuadPart = kPeriod100ns;

	while (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) && WaitForSingleObject(timer, INFINITE) ==
		WAIT_OBJECT_0)
	{
		SampleOnce();
	}

	CloseHandle(timer);
	return 0;
}

void AdoptCallingThread()
{
	if (g_targetId != 0)
		return;

	g_targetId = GetCurrentThreadId();
	g_targetStackBase = reinterpret_cast<uintptr_t>(reinterpret_cast<NT_TIB*>(NtCurrentTeb())->StackBase);
	g_target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, g_targetId);

	LOG("StallSampler: sampling thread %lu, the one that presents", g_targetId);
}

class Harvest
{
public:
	Harvest()
	{
		InterlockedExchange(&g_harvesting, 1);

		while (InterlockedCompareExchange(&g_sampling, 0, 0) != 0)
			YieldProcessor();
	}

	~Harvest()
	{
		InterlockedExchange(&g_harvesting, 0);
	}
};

}

bool StallSampler::Start()
{
	if (InterlockedExchange(&g_running, 1) != 0)
		return true;

	QueryPerformanceFrequency(&g_frequency);

	if (!LearnImage())
	{
		LOG("StallSampler: the game's code section was not found, nothing is sampled");
		return false;
	}

	LearnModules();

	const HANDLE thread = CreateThread(nullptr, 0, &SamplerThread, nullptr, 0, nullptr);

	if (thread == nullptr)
		return false;

	SetThreadPriority(thread, THREAD_PRIORITY_TIME_CRITICAL);
	CloseHandle(thread);

	LOG("StallSampler: on, a frame past %.0f ms is sampled every millisecond", kArmAfterMs);
	return true;
}

std::string StallSampler::Report()
{
	if (g_running == 0)
		return std::string();

	Harvest harvest;

	return StallProfile::Describe(g_table, kReportedSites);
}

void StallSampler::OnPresent()
{
	if (g_running == 0)
		return;

	AdoptCallingThread();

	Harvest harvest;
	g_table.Clear();
	g_frameStart = Now();
}
