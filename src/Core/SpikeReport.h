#pragma once

#include <cstdint>
#include <string>

namespace SpikeReport
{
	constexpr int kReportsPerSecond = 5;

	enum Cost
	{
		Cost_TextureLoad = 0,
		Cost_ShaderCreate,
		Cost_BufferCreate,
		Cost_GameFileRead,
		Cost_OtherFileRead,
		Cost_PresentWait,
		Cost_GameUpdate,
		Cost_COUNT,
	};

	struct Spent
	{
		double ms;
		uint32_t calls;
		uint64_t bytes;
	};

	struct Frame
	{
		double intervalMs;
		Spent costs[Cost_COUNT];
		double modMs;
		const char* slowestTask;
		double slowestTaskMs;
		int suppressed;
	};

	bool IsSpike(double intervalMs, int thresholdMs);
	const char* CostName(Cost cost);
	std::string Describe(const Frame& frame);

	class Limiter
	{
	public:
		bool Allow(uint32_t nowMs);
		int TakeSuppressed();

	private:
		uint32_t m_secondStart = 0;
		int m_allowed = 0;
		int m_suppressed = 0;
		bool m_started = false;
	};
}
