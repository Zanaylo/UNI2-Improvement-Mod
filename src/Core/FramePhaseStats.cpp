#include "Core/FramePhaseStats.h"

#include <cstdarg>
#include <cstdio>

namespace {

bool IsRanked(int site, const FramePhaseStats::Layout& layout)
{
	return site != layout.bodySite && site != layout.waitSite;
}

uint64_t TicksAt(const uint64_t* ticks, int site)
{
	return site < 0 ? 0 : ticks[site];
}

uint64_t Above(uint64_t total, uint64_t part)
{
	return total > part ? total - part : 0;
}

double Ms(uint64_t ticks, double ticksPerMs)
{
	return ticksPerMs > 0.0 ? static_cast<double>(ticks) / ticksPerMs : 0.0;
}

void Append(std::string& text, const char* format, ...)
{
	char line[256];
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(line, sizeof(line), format, arguments);
	va_end(arguments);
	text += line;
}

const char* LabelOf(const char* const* labels, int site)
{
	return labels != nullptr && labels[site] != nullptr ? labels[site] : "?";
}

}

FramePhaseStats::Split FramePhaseStats::SplitFrame(const Frame& frame, const Layout& layout)
{
	Split split = {};
	const uint64_t body = TicksAt(frame.siteTicks, layout.bodySite);
	uint64_t inside = 0;

	for (int site = 0; site < layout.siteCount; ++site)
	{
		if (site != layout.bodySite)
			inside += frame.siteTicks[site];
	}

	split.outside = layout.bodySite < 0 ? 0 : Above(frame.intervalTicks, body);
	split.untimed = Above(layout.bodySite < 0 ? frame.intervalTicks : body, inside);
	split.wait = TicksAt(frame.siteTicks, layout.waitSite);
	return split;
}

int FramePhaseStats::Heaviest(const uint64_t* ticks, const Layout& layout, int* out, int capacity)
{
	int count = 0;

	for (int site = 0; site < layout.siteCount; ++site)
	{
		if (!IsRanked(site, layout) || ticks[site] == 0)
			continue;

		if (count == capacity && ticks[site] <= ticks[out[count - 1]])
			continue;

		int at = count < capacity ? count++ : capacity - 1;
		out[at] = site;

		while (at > 0 && ticks[out[at - 1]] < ticks[out[at]])
		{
			const int swapped = out[at - 1];
			out[at - 1] = out[at];
			out[at] = swapped;
			--at;
		}
	}

	return count;
}

std::string FramePhaseStats::DescribeFrame(const Frame& frame, const Layout& layout, const char* const* labels,
	double ticksPerMs)
{
	const Split split = SplitFrame(frame, layout);
	std::string text;

	Append(text, "frame phases: slow frame %.1f ms, outside the frame %.2f ms, frame wait %.2f ms, untimed %.2f ms:",
		Ms(frame.intervalTicks, ticksPerMs), Ms(split.outside, ticksPerMs), Ms(split.wait, ticksPerMs),
		Ms(split.untimed, ticksPerMs));

	int heaviest[kListed];
	const int count = Heaviest(frame.siteTicks, layout, heaviest, kListed);

	for (int i = 0; i < count; ++i)
	{
		const int site = heaviest[i];
		Append(text, "%s %s %.2f ms (%u)", i == 0 ? "" : ",", LabelOf(labels, site),
			Ms(frame.siteTicks[site], ticksPerMs), frame.siteCalls[site]);
	}

	return text;
}

void FramePhaseStats::Window::Reset(const Layout& layout)
{
	*this = Window();
	m_layout = layout;

	if (m_layout.siteCount > kMostSites)
		m_layout.siteCount = kMostSites;
}

void FramePhaseStats::Window::Add(const Frame& frame, double ticksPerMs)
{
	const uint64_t busy = static_cast<uint64_t>(kBusyFrameMs * ticksPerMs);
	const Split split = SplitFrame(frame, m_layout);

	for (int site = 0; site < m_layout.siteCount; ++site)
	{
		Phase& phase = m_phases[site];
		const uint64_t ticks = frame.siteTicks[site];

		phase.ticks += ticks;
		phase.calls += frame.siteCalls[site];

		if (ticks > phase.worstTicks)
			phase.worstTicks = ticks;

		if (ticks >= busy)
			++phase.busyFrames;
	}

	if (m_layout.waitSite >= 0 && frame.siteCalls[m_layout.waitSite] > 0)
	{
		++m_waitedFrames;

		if (split.wait < m_leastWait)
			m_leastWait = split.wait;
	}

	if (frame.intervalTicks > m_worstInterval)
		m_worstInterval = frame.intervalTicks;

	if (split.outside > m_worstOutside)
		m_worstOutside = split.outside;

	if (split.untimed > m_worstUntimed)
		m_worstUntimed = split.untimed;

	++m_frames;
}

int FramePhaseStats::Window::Frames() const
{
	return m_frames;
}

const FramePhaseStats::Phase& FramePhaseStats::Window::At(int site) const
{
	return m_phases[site];
}

std::string FramePhaseStats::Window::Describe(const char* const* labels, double ticksPerMs) const
{
	std::string text;
	const double frames = m_frames > 0 ? static_cast<double>(m_frames) : 1.0;

	Append(text, "frame phases: %d frames, worst %.1f ms, least frame wait %.2f ms in %d waits, "
		"worst outside the frame %.2f ms, worst untimed %.2f ms; worst phases:", m_frames,
		Ms(m_worstInterval, ticksPerMs), m_waitedFrames > 0 ? Ms(m_leastWait, ticksPerMs) : 0.0, m_waitedFrames,
		Ms(m_worstOutside, ticksPerMs), Ms(m_worstUntimed, ticksPerMs));

	uint64_t worst[kMostSites] = {};

	for (int site = 0; site < m_layout.siteCount; ++site)
		worst[site] = m_phases[site].worstTicks;

	int heaviest[kListed];
	const int count = Heaviest(worst, m_layout, heaviest, kListed);

	for (int i = 0; i < count; ++i)
	{
		const Phase& phase = m_phases[heaviest[i]];
		Append(text, "%s %s worst %.2f mean %.3f ms, %u frames over %.0f ms", i == 0 ? "" : ";",
			LabelOf(labels, heaviest[i]), Ms(phase.worstTicks, ticksPerMs), Ms(phase.ticks, ticksPerMs) / frames,
			phase.busyFrames, kBusyFrameMs);
	}

	return text;
}
