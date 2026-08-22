// A super flash is nobody's move: the bar sits it out, so its length is only readable here.

#pragma once

#include "Game/PlayerState.h"

#include <cstdint>

class SuperFlash
{
public:
	static constexpr int kPlayers = 2;

	void Reset();
	void Update(const PlayerState::State* states, const bool* valid, int count);

	bool IsRunning() const { return m_running; }
	bool IsFreezing() const { return m_freezing; }
	int GetOwner() const { return m_owner; }
	int GetFrames(int player) const;

private:
	struct Reading
	{
		bool flagsUp;
		bool anyoneStopped;
		bool stoppedMoved;
		bool anyoneMoved;
		int owner;
	};

	Reading Read(const PlayerState::State* states, const bool* valid, int count) const;
	bool Advanced(int player, const PlayerState::State& state) const;
	void Begin(int owner);
	void Advance(const Reading& reading);
	void Remember(const PlayerState::State* states, const bool* valid, int count);

	bool m_running = false;
	bool m_freezing = false;
	int m_owner = -1;
	int m_frames[kPlayers] = {};
	uint32_t m_moveFrame[kPlayers] = {};
	bool m_hasMoveFrame[kPlayers] = {};
};
