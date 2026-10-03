#include "Core/Harness/PerfCommands.h"

#include "Core/Boot/Modules.h"
#include "Core/Profiler.h"
#include "D3D9/Device/DeviceHooks.h"

#include <Windows.h>
#include <psapi.h>
#include <d3d9.h>

#include <cstdio>

namespace {

constexpr int kSlowestTasks = 12;

std::string Describe(const char* label, const Profiler::Stats& stats)
{
	char text[192] = {};
	sprintf_s(text, "%s avg %.2fms median %.2fms p99 %.2fms max %.2fms slow %d/%d", label, stats.averageMs,
		stats.medianMs, stats.p99Ms, stats.maxMs, stats.slowFrames, stats.samples);

	return text;
}

std::string Sections()
{
	std::string out;

	for (int i = 0; i < Profiler::Section_COUNT; ++i)
	{
		const Profiler::Section section = static_cast<Profiler::Section>(i);
		char text[64] = {};
		sprintf_s(text, "|%s %.3fms", Profiler::GetSectionName(section), Profiler::GetSectionMs(section));
		out += text;
	}

	return out;
}

std::string Histogram()
{
	const int edges[] = { 17, 20, 25, 34 };
	int counts[5] = {};

	for (int i = 0; i < Profiler::kHistogramBuckets; ++i)
	{
		int band = 0;

		while (band < 4 && i >= edges[band])
			++band;

		counts[band] += Profiler::GetHistogramBucket(i);
	}

	char text[128] = {};
	sprintf_s(text, "|frames <17ms %d, 17-20 %d, 20-25 %d, 25-34 %d, 34+ %d", counts[0], counts[1],
		counts[2], counts[3], counts[4]);

	return text;
}

std::string Memory()
{
	PROCESS_MEMORY_COUNTERS_EX counters = {};
	counters.cb = sizeof(counters);

	if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
		sizeof(counters)))
		return "error the memory counters could not be read";

	MEMORYSTATUSEX status = {};
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);

	DWORD handles = 0;
	GetProcessHandleCount(GetCurrentProcess(), &handles);

	IDirect3DDevice9* const device = DeviceHooks::GetDevice();
	const unsigned int textureMemory = device != nullptr ? device->GetAvailableTextureMem() : 0;

	char text[256] = {};
	sprintf_s(text, "ok private %zu working %zu virtual %llu handles %lu texture_free %u", counters.PrivateUsage,
		counters.WorkingSetSize, status.ullTotalVirtual - status.ullAvailVirtual, handles, textureMemory);

	return text;
}

std::string Start()
{
	Profiler::SetEnabled(true);
	Profiler::Reset();
	Modules::ResetTaskTimes();

	return "ok measuring";
}

std::string Report()
{
	if (!Profiler::IsEnabled())
		return "error perf start first";

	return "ok " + Describe("present", Profiler::GetPresentStats()) + "|" +
		Describe("tick", Profiler::GetTickStats()) + Histogram() + Sections() +
		Modules::SlowestTasks(kSlowestTasks);
}

}

bool PerfCommands::Execute(const std::vector<std::string>& words, std::string& reply)
{
	if (words[0] != "perf")
		return false;

	if (words.size() >= 2 && words[1] == "start")
	{
		reply = Start();
		return true;
	}

	if (words.size() >= 2 && words[1] == "memory")
	{
		reply = Memory();
		return true;
	}

	if (words.size() >= 2 && words[1] == "stop")
	{
		Profiler::SetEnabled(false);
		reply = "ok stopped";
		return true;
	}

	reply = Report();
	return true;
}
