#include "Network/GameEnding.h"

namespace {

constexpr GameEnding::Step kNothing = { false, GameEnding::Cause::None, GameEnding::kNoWinner };
constexpr GameEnding::Step kCaptureOnly = { true, GameEnding::Cause::None, GameEnding::kNoWinner };

bool IsAtOrAfter(uint32_t moment, uint32_t reference)
{
	return static_cast<int32_t>(moment - reference) >= 0;
}

}

GameEnding::Step GameEnding::Update(const Frame& frame)
{
	switch (m_phase)
	{
	case Phase::Running:
		return WhileRunning(frame);
	case Phase::Finished:
		return WhileFinished(frame);
	case Phase::Interrupted:
		return WhileInterrupted(frame);
	default:
		return WhileIdle(frame);
	}
}

void GameEnding::Reset()
{
	m_phase = Phase::Idle;
}

GameEnding::Step GameEnding::WhileIdle(const Frame& frame)
{
	if (!frame.inSession || !frame.gameRunning)
		return kNothing;

	m_phase = Phase::Running;
	m_startedAt = frame.nowMs;
	return kNothing;
}

GameEnding::Step GameEnding::WhileRunning(const Frame& frame)
{
	if (frame.decidedWinner != kNoWinner)
		return End(Cause::Decided, frame.decidedWinner, true, Phase::Finished);

	if (OpponentLeft(frame))
		return End(Cause::OpponentLeft, frame.localSide, true, Phase::Finished);

	if (frame.inSession && frame.gameRunning)
		return kNothing;

	m_phase = Phase::Interrupted;
	m_interruptedAt = frame.nowMs;
	return kCaptureOnly;
}

GameEnding::Step GameEnding::WhileFinished(const Frame& frame)
{
	if (!frame.inSession || !frame.gameRunning)
		m_phase = Phase::Idle;

	return kNothing;
}

GameEnding::Step GameEnding::WhileInterrupted(const Frame& frame)
{
	if (OpponentLeft(frame))
		return End(Cause::OpponentLeft, frame.localSide, false, Phase::Idle);

	if (frame.nowMs - m_interruptedAt < kForfeitWindowMs)
		return kNothing;

	return End(Cause::ConnectionLost, kNoWinner, false, Phase::Idle);
}

bool GameEnding::OpponentLeft(const Frame& frame) const
{
	return frame.opponentLeftAt != 0 && IsAtOrAfter(frame.opponentLeftAt, m_startedAt);
}

GameEnding::Step GameEnding::End(Cause cause, int winner, bool captureNow, Phase next)
{
	m_phase = next;
	return { captureNow, cause, winner };
}
