#pragma once

#include <cstdint>
#include <string>

namespace FramePhaseStats
{
	constexpr int kMostSites = 256;
	constexpr int kListed = 8;
	constexpr double kBusyFrameMs = 1.0;

	struct Layout
	{
		int siteCount;
		int bodySite;
		int waitSite;
	};

	struct Frame
	{
		const uint64_t* siteTicks;
		const uint32_t* siteCalls;
		uint64_t intervalTicks;
	};

	struct Split
	{
		uint64_t outside;
		uint64_t untimed;
		uint64_t wait;
	};

	struct Phase
	{
		uint64_t ticks;
		uint64_t worstTicks;
		uint32_t calls;
		uint32_t busyFrames;
	};

	Split SplitFrame(const Frame& frame, const Layout& layout);
	int Heaviest(const uint64_t* ticks, const Layout& layout, int* out, int capacity);
	std::string DescribeFrame(const Frame& frame, const Layout& layout, const char* const* labels,
		double ticksPerMs);

	class Window
	{
	public:
		void Reset(const Layout& layout);
		void Add(const Frame& frame, double ticksPerMs);

		int Frames() const;
		const Phase& At(int site) const;

		std::string Describe(const char* const* labels, double ticksPerMs) const;

	private:
		Layout m_layout = {};
		Phase m_phases[kMostSites] = {};
		int m_frames = 0;
		int m_waitedFrames = 0;
		uint64_t m_worstInterval = 0;
		uint64_t m_worstOutside = 0;
		uint64_t m_worstUntimed = 0;
		uint64_t m_leastWait = UINT64_MAX;
	};
}
