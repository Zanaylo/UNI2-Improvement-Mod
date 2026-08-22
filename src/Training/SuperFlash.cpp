#include "Training/SuperFlash.h"

#include "Game/GameOffsets.h"

namespace {

constexpr int kMaxFrames = 0xffff;

}

void SuperFlash::Reset()
{
	m_running = false;
	m_freezing = false;
	m_owner = -1;

	for (int player = 0; player < kPlayers; ++player)
	{
		m_frames[player] = 0;
		m_moveFrame[player] = 0;
		m_hasMoveFrame[player] = false;
	}
}

int SuperFlash::GetFrames(int player) const
{
	if (player < 0 || player >= kPlayers)
		return 0;

	return m_frames[player];
}

bool SuperFlash::Advanced(int player, const PlayerState::State& state) const
{
	if (!m_hasMoveFrame[player])
		return true;

	return state.mvCountFrame != m_moveFrame[player];
}

SuperFlash::Reading SuperFlash::Read(const PlayerState::State* states, const bool* valid,
	int count) const
{
	Reading reading = {};
	reading.owner = -1;

	for (int player = 0; player < count && player < kPlayers; ++player)
	{
		if (!valid[player])
			continue;

		const PlayerState::State& state = states[player];
		const bool moved = Advanced(player, state);

		reading.anyoneMoved = reading.anyoneMoved || moved;

		if ((state.moveCodeEx[3] & GameOffsets::kMoveCode3Anten) != 0)
		{
			reading.flagsUp = true;

			if (reading.owner < 0)
				reading.owner = player;
		}

		if ((state.moveCodeEx[2] & GameOffsets::kMoveCode2EnemyAntenStop) == 0)
			continue;

		reading.flagsUp = true;
		reading.anyoneStopped = true;
		reading.stoppedMoved = reading.stoppedMoved || moved;
	}

	return reading;
}

void SuperFlash::Begin(int owner)
{
	m_running = true;
	m_owner = owner;

	if (owner < 0)
		return;

	m_frames[owner] = 0;
}

void SuperFlash::Advance(const Reading& reading)
{
	if (!reading.flagsUp)
	{
		m_running = false;
		m_freezing = false;
		return;
	}

	if (!m_running || m_owner < 0)
		Begin(reading.owner);

	m_freezing = reading.anyoneStopped ? !reading.stoppedMoved : !reading.anyoneMoved;

	if (!m_freezing)
		return;

	if (m_owner < 0)
		return;

	if (m_frames[m_owner] >= kMaxFrames)
		return;

	++m_frames[m_owner];
}

void SuperFlash::Remember(const PlayerState::State* states, const bool* valid, int count)
{
	for (int player = 0; player < kPlayers; ++player)
	{
		const bool sampled = player < count && valid[player];

		m_hasMoveFrame[player] = sampled;
		m_moveFrame[player] = sampled ? states[player].mvCountFrame : 0;
	}
}

void SuperFlash::Update(const PlayerState::State* states, const bool* valid, int count)
{
	Advance(Read(states, valid, count));
	Remember(states, valid, count);
}
