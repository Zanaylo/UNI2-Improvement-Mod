#pragma once

#include <cstdint>

class GameEnding
{
public:
	static constexpr int kNoWinner = -1;
	static constexpr uint32_t kForfeitWindowMs = 15000;

	enum class Cause
	{
		None,
		Decided,
		OpponentLeft,
		ConnectionLost
	};

	struct Frame
	{
		bool inSession;
		bool gameRunning;
		int decidedWinner;
		uint32_t opponentLeftAt;
		int localSide;
		uint32_t nowMs;
	};

	struct Step
	{
		bool captureNow;
		Cause ended;
		int winner;
	};

	Step Update(const Frame& frame);
	void Reset();

private:
	enum class Phase
	{
		Idle,
		Running,
		Finished,
		Interrupted
	};

	Step WhileIdle(const Frame& frame);
	Step WhileRunning(const Frame& frame);
	Step WhileFinished(const Frame& frame);
	Step WhileInterrupted(const Frame& frame);

	bool OpponentLeft(const Frame& frame) const;
	Step End(Cause cause, int winner, bool captureNow, Phase next);

	Phase m_phase = Phase::Idle;
	uint32_t m_startedAt = 0;
	uint32_t m_interruptedAt = 0;
};
