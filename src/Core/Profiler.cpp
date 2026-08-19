#include "Core/Profiler.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "D3D9/DeviceHooks.h"
#include "D3D9/PresentTuning.h"
#include "Game/PumpWait.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kWindowFrames = 240;
constexpr double kSlowFrameMs = 20.0;
constexpr double kBucketMs = 1.0;
constexpr double kSmoothing = 0.1;

const char* const kSectionNames[Profiler::Section_COUNT] = {
	"Present/OnlineState",
	"Present/ReplayState",
	"Present/FrozenFrame",
	"Present/FrameMeterHud",
	"Present/Overlay",
	"Present/Palette",
	"Present/PaletteShare",
	"Present/oPresent",
	"Tick/DummyRecorder",
	"Tick/PlayerState",
	"EffectScan (both threads)",
	"Tick/FrameMeter",
	"Tick/StateRecorder",
	"Tick/oFrameUpdate",
};

bool g_enabled = false;
double g_ticksToMs = 0.0;

int64_t g_pending[Profiler::Section_COUNT] = {};
int64_t g_pendingBlock = 0;
double g_sectionMs[Profiler::Section_COUNT] = {};

double g_presentSamples[kWindowFrames] = {};
int g_presentCount = 0;
int g_presentCursor = 0;
int64_t g_lastPresent = 0;

double g_tickSamples[kWindowFrames] = {};
int g_tickCount = 0;
int g_tickCursor = 0;
int64_t g_lastTick = 0;

int g_histogram[Profiler::kHistogramBuckets] = {};
int g_fineHistogram[Profiler::kFineBuckets] = {};

constexpr int kStampWindow = 256;
int64_t g_presentStamps[kStampWindow] = {};
int g_stampCount = 0;
int g_stampCursor = 0;

double g_presentBlockSamples[kWindowFrames] = {};
int g_presentBlockCount = 0;
int g_presentBlockCursor = 0;

bool g_hasBaseline = false;
char g_baselineLabel[128] = {};
Profiler::Stats g_baselinePresent = {};
Profiler::Stats g_baselineTick = {};
double g_baselineSectionMs[Profiler::Section_COUNT] = {};

double TicksToMs()
{
	if (g_ticksToMs != 0.0)
		return g_ticksToMs;

	LARGE_INTEGER frequency = {};
	if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0)
		return 0.0;

	g_ticksToMs = 1000.0 / static_cast<double>(frequency.QuadPart);
	return g_ticksToMs;
}

void PushSample(double* samples, int& count, int& cursor, double valueMs)
{
	samples[cursor] = valueMs;
	cursor = (cursor + 1) % kWindowFrames;

	if (count < kWindowFrames)
		++count;
}

Profiler::Stats Summarise(const double* samples, int count)
{
	Profiler::Stats stats = {};
	stats.samples = count;

	if (count == 0)
		return stats;

	double sorted[kWindowFrames] = {};
	double total = 0.0;

	for (int i = 0; i < count; ++i)
	{
		sorted[i] = samples[i];
		total += samples[i];

		if (samples[i] > kSlowFrameMs)
			++stats.slowFrames;
	}

	std::sort(sorted, sorted + count);

	stats.averageMs = total / count;
	stats.medianMs = sorted[count / 2];
	stats.p99Ms = sorted[count - 1 - (count / 100)];
	stats.maxMs = sorted[count - 1];

	double variance = 0.0;
	double absolute = 0.0;
	int onTarget = 0;

	for (int i = 0; i < count; ++i)
	{
		const double fromMean = samples[i] - stats.averageMs;
		variance += fromMean * fromMean;

		const double fromTarget = samples[i] - Profiler::kTargetMs;
		absolute += fromTarget < 0.0 ? -fromTarget : fromTarget;

		if (fromTarget > -0.5 && fromTarget < 0.5)
			++onTarget;
	}

	stats.stddevMs = std::sqrt(variance / count);
	stats.madMs = absolute / count;
	stats.onTargetPercent = 100.0 * static_cast<double>(onTarget) / static_cast<double>(count);
	return stats;
}

}

void Profiler::SetEnabled(bool enabled)
{
	if (enabled && TicksToMs() == 0.0)
		return;

	if (g_enabled == enabled)
		return;

	g_enabled = enabled;
	Reset();

	LOG("Profiler %s", enabled ? "enabled" : "disabled");
}

bool Profiler::IsEnabled()
{
	return g_enabled;
}

int64_t Profiler::Now()
{
	LARGE_INTEGER counter = {};
	QueryPerformanceCounter(&counter);
	return counter.QuadPart;
}

void Profiler::Add(Section section, int64_t elapsedTicks)
{
	if (section < 0 || section >= Section_COUNT)
		return;

	g_pending[section] += elapsedTicks;

	if (section == Section_PresentDevice)
		g_pendingBlock += elapsedTicks;
}

void Profiler::EndPresentFrame()
{
	if (!g_enabled)
		return;

	const double toMs = TicksToMs();

	for (int i = 0; i < Section_COUNT; ++i)
	{
		const double frameMs = static_cast<double>(g_pending[i]) * toMs;
		g_sectionMs[i] += (frameMs - g_sectionMs[i]) * kSmoothing;
		g_pending[i] = 0;
	}

	const int64_t now = Now();

	if (g_lastPresent != 0)
	{
		const double intervalMs = static_cast<double>(now - g_lastPresent) * toMs;
		PushSample(g_presentSamples, g_presentCount, g_presentCursor, intervalMs);

		int bucket = static_cast<int>(intervalMs / kBucketMs);
		if (bucket < 0)
			bucket = 0;
		if (bucket >= kHistogramBuckets)
			bucket = kHistogramBuckets - 1;

		++g_histogram[bucket];

		int fine = static_cast<int>((intervalMs - (kTargetMs - 5.0)) / kFineBucketMs);
		if (fine < 0)
			fine = 0;
		if (fine >= kFineBuckets)
			fine = kFineBuckets - 1;

		++g_fineHistogram[fine];
	}

	g_presentStamps[g_stampCursor] = now;
	g_stampCursor = (g_stampCursor + 1) % kStampWindow;
	if (g_stampCount < kStampWindow)
		++g_stampCount;

	PushSample(g_presentBlockSamples, g_presentBlockCount, g_presentBlockCursor,
		static_cast<double>(g_pendingBlock) * toMs);
	g_pendingBlock = 0;

	g_lastPresent = now;
}

void Profiler::EndTickFrame()
{
	if (!g_enabled)
		return;

	const int64_t now = Now();

	if (g_lastTick != 0)
	{
		const double intervalMs = static_cast<double>(now - g_lastTick) * TicksToMs();
		PushSample(g_tickSamples, g_tickCount, g_tickCursor, intervalMs);
	}

	g_lastTick = now;
}

const char* Profiler::GetSectionName(Section section)
{
	if (section < 0 || section >= Section_COUNT)
		return "";

	return kSectionNames[section];
}

double Profiler::GetSectionMs(Section section)
{
	if (section < 0 || section >= Section_COUNT)
		return 0.0;

	return g_sectionMs[section];
}

Profiler::Stats Profiler::GetPresentStats()
{
	return Summarise(g_presentSamples, g_presentCount);
}

Profiler::Stats Profiler::GetTickStats()
{
	return Summarise(g_tickSamples, g_tickCount);
}

int Profiler::GetHistogramBucket(int index)
{
	if (index < 0 || index >= kHistogramBuckets)
		return 0;

	return g_histogram[index];
}

void Profiler::CaptureBaseline(const char* label)
{
	g_baselinePresent = GetPresentStats();
	g_baselineTick = GetTickStats();

	for (int i = 0; i < Section_COUNT; ++i)
		g_baselineSectionMs[i] = g_sectionMs[i];

	strncpy_s(g_baselineLabel, label != nullptr ? label : "", _TRUNCATE);
	g_hasBaseline = g_baselinePresent.samples > 0;

	LOG("Profiler baseline captured: %s (%d samples, median %.2fms)", g_baselineLabel,
		g_baselinePresent.samples, g_baselinePresent.medianMs);
}

void Profiler::ClearBaseline()
{
	g_hasBaseline = false;
	g_baselineLabel[0] = 0;
}

bool Profiler::HasBaseline()
{
	return g_hasBaseline;
}

const char* Profiler::GetBaselineLabel()
{
	return g_baselineLabel;
}

Profiler::Stats Profiler::GetBaselinePresentStats()
{
	return g_baselinePresent;
}

Profiler::Stats Profiler::GetBaselineTickStats()
{
	return g_baselineTick;
}

double Profiler::GetBaselineSectionMs(Section section)
{
	if (section < 0 || section >= Section_COUNT)
		return 0.0;

	return g_baselineSectionMs[section];
}

int Profiler::GetSampleCount()
{
	return g_presentCount;
}

void Profiler::Reset()
{
	for (int i = 0; i < Section_COUNT; ++i)
	{
		g_pending[i] = 0;
		g_sectionMs[i] = 0.0;
	}

	for (int i = 0; i < kHistogramBuckets; ++i)
		g_histogram[i] = 0;

	g_presentCount = 0;
	g_presentCursor = 0;
	g_lastPresent = 0;

	g_tickCount = 0;
	g_tickCursor = 0;
	g_lastTick = 0;

	for (int i = 0; i < kFineBuckets; ++i)
		g_fineHistogram[i] = 0;

	g_stampCount = 0;
	g_stampCursor = 0;

	g_presentBlockCount = 0;
	g_presentBlockCursor = 0;
	g_pendingBlock = 0;

}

int Profiler::GetFineHistogramBucket(int index)
{
	if (index < 0 || index >= kFineBuckets)
		return 0;

	return g_fineHistogram[index];
}

double Profiler::GetFineHistogramBaseMs()
{
	return kTargetMs - 5.0;
}

bool Profiler::FindModes(double& outFirstMs, double& outSecondMs, double& outSeparationMs)
{
	outFirstMs = 0.0;
	outSecondMs = 0.0;
	outSeparationMs = 0.0;

	int first = -1;
	int second = -1;

	for (int i = 0; i < kFineBuckets; ++i)
	{
		if (first < 0 || g_fineHistogram[i] > g_fineHistogram[first])
			first = i;
	}

	if (first < 0 || g_fineHistogram[first] == 0)
		return false;

	// A neighbouring bucket is the same mode, not a second one.
	for (int i = 0; i < kFineBuckets; ++i)
	{
		if (i >= first - 2 && i <= first + 2)
			continue;

		if (second < 0 || g_fineHistogram[i] > g_fineHistogram[second])
			second = i;
	}

	if (second < 0 || g_fineHistogram[second] == 0)
		return false;

	const double base = GetFineHistogramBaseMs();
	outFirstMs = base + (first + 0.5) * kFineBucketMs;
	outSecondMs = base + (second + 0.5) * kFineBucketMs;
	outSeparationMs = outFirstMs > outSecondMs ? outFirstMs - outSecondMs : outSecondMs - outFirstMs;

	// Only worth reporting when the second cluster is a real population rather than a tail.
	return g_fineHistogram[second] * 4 >= g_fineHistogram[first];
}

double Profiler::GetPresentedFps()
{
	if (g_stampCount < 2)
		return 0.0;

	const double toMs = TicksToMs();
	if (toMs == 0.0)
		return 0.0;

	const int newest = (g_stampCursor + kStampWindow - 1) % kStampWindow;
	const int64_t last = g_presentStamps[newest];

	int counted = 0;
	int64_t oldest = last;

	for (int i = 1; i < g_stampCount; ++i)
	{
		const int index = (newest + kStampWindow - i) % kStampWindow;
		const double ageMs = static_cast<double>(last - g_presentStamps[index]) * toMs;

		if (ageMs > 2000.0)
			break;

		oldest = g_presentStamps[index];
		counted = i;
	}

	if (counted == 0)
		return 0.0;

	const double spanMs = static_cast<double>(last - oldest) * toMs;
	if (spanMs <= 0.0)
		return 0.0;

	return static_cast<double>(counted) * 1000.0 / spanMs;
}

Profiler::Stats Profiler::GetPresentBlockStats()
{
	return Summarise(g_presentBlockSamples, g_presentBlockCount);
}

void Profiler::BuildSummary(char* out, size_t size)
{
	if (out == nullptr || size == 0)
		return;

	const D3DPRESENT_PARAMETERS& present = DeviceHooks::GetPresentParameters();
	const Stats frame = GetPresentStats();
	const Stats block = GetPresentBlockStats();

	double firstMs = 0.0;
	double secondMs = 0.0;
	double separationMs = 0.0;
	const bool bimodal = FindModes(firstMs, secondMs, separationMs);

	double waitMedianUs = 0.0;
	double waitP99Us = 0.0;
	int waitSamples = 0;
	PumpWait::GetWaitStats(waitMedianUs, waitP99Us, waitSamples);

	snprintf(out, size,
		"display    %ux%u %s, %u Hz, %u back buffer(s), interval 0x%08lx\n"
		"tuning     %s\n"
		"options    timer=%d throttling=%d displayTuning=%d extraBuffer=%d pumpWait=%d(%s)\n"
		"interval   median %.2f ms, mean %.2f ms, sd %.2f ms, |off target| %.2f ms, on target %.0f%%\n"
		"           p99 %.2f ms, worst %.2f ms, over 20 ms %d of %d, presenting %.1f fps\n"
		"modes      %s%.2f / %.2f ms, %.2f ms apart\n"
		"present    median %.2f ms, p99 %.2f ms\n"
		"pumpwait   median %.0f us, p99 %.0f us over %d waits\n",
		present.BackBufferWidth, present.BackBufferHeight,
		present.Windowed ? "windowed" : "fullscreen", present.FullScreen_RefreshRateInHz,
		present.BackBufferCount, static_cast<unsigned long>(present.PresentationInterval),
		PresentTuning::GetLastDecision(),
		g_modVals.timerResolution ? 1 : 0, g_modVals.powerThrottlingOptOut ? 1 : 0,
		g_modVals.displayTuning ? 1 : 0, g_modVals.extraBackBuffer ? 1 : 0,
		g_modVals.pumpWait ? 1 : 0, g_modVals.pumpWaitAllInput ? "all input" : "handshake",
		frame.medianMs, frame.averageMs, frame.stddevMs, frame.madMs, frame.onTargetPercent,
		frame.p99Ms, frame.maxMs, frame.slowFrames, frame.samples, GetPresentedFps(),
		bimodal ? "" : "single cluster, ", firstMs, secondMs, separationMs,
		block.medianMs, block.p99Ms,
		waitMedianUs, waitP99Us, waitSamples);
}

bool Profiler::ExportCsv(const char* path)
{
	if (path == nullptr)
		return false;

	FILE* file = nullptr;
	if (fopen_s(&file, path, "w") != 0 || file == nullptr)
		return false;

	fprintf(file, "what,value\n");

	const Stats frame = GetPresentStats();
	fprintf(file, "interval_median_ms,%.4f\n", frame.medianMs);
	fprintf(file, "interval_mean_ms,%.4f\n", frame.averageMs);
	fprintf(file, "interval_stddev_ms,%.4f\n", frame.stddevMs);
	fprintf(file, "interval_mad_ms,%.4f\n", frame.madMs);
	fprintf(file, "interval_on_target_percent,%.2f\n", frame.onTargetPercent);
	fprintf(file, "interval_p99_ms,%.4f\n", frame.p99Ms);
	fprintf(file, "interval_max_ms,%.4f\n", frame.maxMs);
	fprintf(file, "frames_over_20ms,%d\n", frame.slowFrames);
	fprintf(file, "samples,%d\n", frame.samples);
	fprintf(file, "presented_fps,%.3f\n", GetPresentedFps());

	const double base = GetFineHistogramBaseMs();
	for (int i = 0; i < kFineBuckets; ++i)
		fprintf(file, "fine_%.2fms,%d\n", base + (i + 0.5) * kFineBucketMs, g_fineHistogram[i]);

	for (int i = 0; i < kHistogramBuckets; ++i)
		fprintf(file, "coarse_%dms,%d\n", i, g_histogram[i]);

	for (int i = 0; i < Section_COUNT; ++i)
		fprintf(file, "section_%s_ms,%.4f\n", kSectionNames[i], g_sectionMs[i]);

	fclose(file);
	return true;
}

void Profiler::DumpToLog()
{
	const Stats present = GetPresentStats();
	const Stats tick = GetTickStats();

	LOG("Profiler present: avg %.2fms median %.2fms p99 %.2fms max %.2fms slow %d/%d",
		present.averageMs, present.medianMs, present.p99Ms, present.maxMs, present.slowFrames,
		present.samples);

	LOG("Profiler tick: avg %.2fms median %.2fms p99 %.2fms max %.2fms slow %d/%d",
		tick.averageMs, tick.medianMs, tick.p99Ms, tick.maxMs, tick.slowFrames, tick.samples);

	for (int i = 0; i < Section_COUNT; ++i)
		LOG("Profiler %-24s %.3fms", GetSectionName(static_cast<Section>(i)), g_sectionMs[i]);
}
