#pragma once

#include <cstdint>

namespace Profiler
{
	enum Section
	{
		Section_PresentOnline,
		Section_PresentReplay,
		Section_PresentFrozenFrame,
		Section_PresentMeterHud,
		Section_PresentOverlay,
		Section_PresentPalette,
		Section_PresentShare,
		Section_PresentDevice,
		Section_TickDummy,
		Section_TickPlayerState,
		Section_TickEffectScan,
		Section_TickMeter,
		Section_TickRecorder,
		Section_TickGame,
		Section_COUNT
	};

	constexpr int kHistogramBuckets = 40;

	constexpr int kFineBuckets = 40;
	constexpr double kFineBucketMs = 0.25;
	constexpr double kTargetMs = 1000.0 / 60.0;

	struct Stats
	{
		double averageMs;
		double medianMs;
		double p99Ms;
		double maxMs;
		double stddevMs;
		double madMs;
		double onTargetPercent;
		int slowFrames;
		int samples;
	};

	void SetEnabled(bool enabled);
	bool IsEnabled();

	int64_t Now();
	void Add(Section section, int64_t elapsedTicks);

	void EndPresentFrame();
	void EndTickFrame();

	const char* GetSectionName(Section section);
	double GetSectionMs(Section section);

	Stats GetPresentStats();
	Stats GetTickStats();
	int GetHistogramBucket(int index);

	int GetFineHistogramBucket(int index);
	double GetFineHistogramBaseMs();

	bool FindModes(double& outFirstMs, double& outSecondMs, double& outSeparationMs);

	double GetPresentedFps();
	Stats GetPresentBlockStats();

	bool ExportCsv(const char* path);
	void BuildSummary(char* out, size_t size);

	void CaptureBaseline(const char* label);
	void ClearBaseline();
	bool HasBaseline();
	const char* GetBaselineLabel();
	Stats GetBaselinePresentStats();
	Stats GetBaselineTickStats();
	double GetBaselineSectionMs(Section section);

	int GetSampleCount();

	void Reset();
	void DumpToLog();

	class Scope
	{
	public:
		explicit Scope(Section section)
			: m_section(section)
			, m_start(IsEnabled() ? Now() : 0)
		{
		}

		~Scope()
		{
			if (m_start == 0)
				return;

			Add(m_section, Now() - m_start);
		}

		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

	private:
		Section m_section;
		int64_t m_start;
	};
}
