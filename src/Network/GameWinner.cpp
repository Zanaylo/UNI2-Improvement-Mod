#include "Network/GameWinner.h"

namespace {

constexpr int kDraw = -1;
constexpr int kSecondSide = 1;

bool IsWrittenResult(int recorded)
{
	return recorded >= kDraw && recorded <= kSecondSide;
}

}

void GameWinner::Arm()
{
	m_armed = true;
	ForgetCandidate();
}

void GameWinner::Disarm()
{
	m_armed = false;
	ForgetCandidate();
}

int GameWinner::Update(int recorded)
{
	if (!m_armed)
		return kNoWinner;

	if (!IsWrittenResult(recorded))
	{
		ForgetCandidate();
		return kNoWinner;
	}

	m_heldFrames = recorded == m_candidate ? m_heldFrames + 1 : 1;
	m_candidate = recorded;

	if (m_heldFrames < kSettleFrames)
		return kNoWinner;

	Disarm();
	return recorded == kDraw ? kNoWinner : recorded;
}

void GameWinner::ForgetCandidate()
{
	m_candidate = kUnwritten;
	m_heldFrames = 0;
}
