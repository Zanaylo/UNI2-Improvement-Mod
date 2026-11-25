#pragma once

class GameWinner
{
public:
	static constexpr int kNoWinner = -1;
	static constexpr int kUnwritten = -2;
	static constexpr int kSettleFrames = 30;

	void Arm();
	void Disarm();
	int Update(int recorded);

private:
	void ForgetCandidate();

	bool m_armed = false;
	int m_candidate = kUnwritten;
	int m_heldFrames = 0;
};
