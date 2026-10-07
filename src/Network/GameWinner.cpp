#include "Network/GameWinner.h"

namespace {

constexpr int kFirstSide = 0;
constexpr int kSecondSide = 1;

bool IsPosingAsWinner(const GameWinner::Side& side)
{
	return side.pattern == GameWinner::kWinPattern;
}

}

int GameWinner::Update(const Side& first, const Side& second)
{
	if (m_decided)
	{
		m_decided = !(first.vitals.IsFull() && second.vitals.IsFull());
		return kNoWinner;
	}

	const bool firstWon = IsPosingAsWinner(first);

	if (firstWon == IsPosingAsWinner(second))
		return kNoWinner;

	m_decided = true;
	return firstWon ? kFirstSide : kSecondSide;
}

void GameWinner::Reset()
{
	m_decided = false;
}
