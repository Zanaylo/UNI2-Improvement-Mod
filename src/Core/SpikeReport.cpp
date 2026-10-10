#include "Core/SpikeReport.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

constexpr double kNotableMs = 0.05;
constexpr uint32_t kSecondMs = 1000;
constexpr uint64_t kKilobyte = 1024;

const char* const kCostNames[SpikeReport::Cost_COUNT] = {
	"texture loads",
	"shader creation",
	"buffer creation",
	"game file reads",
	"other file reads",
	"present wait",
	"game update",
};

std::string CostText(SpikeReport::Cost cost, const SpikeReport::Spent& spent)
{
	char text[128] = {};

	if (spent.bytes == 0)
	{
		sprintf_s(text, "%s %.1f ms (%u)", kCostNames[cost], spent.ms, spent.calls);
		return text;
	}

	sprintf_s(text, "%s %.1f ms (%u, %llu KB)", kCostNames[cost], spent.ms, spent.calls,
		static_cast<unsigned long long>(spent.bytes / kKilobyte));
	return text;
}

std::string ModText(const SpikeReport::Frame& frame)
{
	char text[128] = {};
	sprintf_s(text, "mod %.1f ms (slowest %s %.1f ms)", frame.modMs,
		frame.slowestTask != nullptr ? frame.slowestTask : "?", frame.slowestTaskMs);

	return text;
}

std::vector<std::string> Parts(const SpikeReport::Frame& frame)
{
	std::vector<SpikeReport::Cost> order;

	for (int cost = 0; cost < SpikeReport::Cost_COUNT; ++cost)
	{
		if (frame.costs[cost].ms >= kNotableMs)
			order.push_back(static_cast<SpikeReport::Cost>(cost));
	}

	std::stable_sort(order.begin(), order.end(), [&frame](SpikeReport::Cost left, SpikeReport::Cost right)
	{
		return frame.costs[left].ms > frame.costs[right].ms;
	});

	std::vector<std::string> parts;

	for (SpikeReport::Cost cost : order)
		parts.push_back(CostText(cost, frame.costs[cost]));

	if (frame.modMs >= kNotableMs)
		parts.push_back(ModText(frame));

	return parts;
}

}

bool SpikeReport::IsSpike(double intervalMs, int thresholdMs)
{
	return thresholdMs > 0 && intervalMs >= thresholdMs;
}

const char* SpikeReport::CostName(Cost cost)
{
	return cost >= 0 && cost < Cost_COUNT ? kCostNames[cost] : "";
}

std::string SpikeReport::Describe(const Frame& frame)
{
	char head[48] = {};
	sprintf_s(head, "spike %.1f ms", frame.intervalMs);

	std::string text = head;
	const std::vector<std::string> parts = Parts(frame);

	if (parts.empty())
		text += ", nothing measured explains it";

	for (size_t i = 0; i < parts.size(); ++i)
		text += (i == 0 ? ": " : ", ") + parts[i];

	if (frame.suppressed <= 0)
		return text;

	char tail[64] = {};
	sprintf_s(tail, " (%d more spike(s) not written)", frame.suppressed);

	return text + tail;
}

bool SpikeReport::Limiter::Allow(uint32_t nowMs)
{
	if (!m_started || nowMs - m_secondStart >= kSecondMs)
	{
		m_started = true;
		m_secondStart = nowMs;
		m_allowed = 0;
	}

	if (m_allowed < kReportsPerSecond)
	{
		++m_allowed;
		return true;
	}

	++m_suppressed;
	return false;
}

int SpikeReport::Limiter::TakeSuppressed()
{
	const int suppressed = m_suppressed;
	m_suppressed = 0;

	return suppressed;
}
