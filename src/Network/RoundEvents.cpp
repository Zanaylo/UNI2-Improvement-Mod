#include "Network/RoundEvents.h"

RoundEvents::Event RoundEvents::Update(const Vitals& first, const Vitals& second)
{
	if (!first.IsRead() || !second.IsRead())
		return Event::None;

	if (!m_knockedOut && (first.IsDown() || second.IsDown()))
	{
		m_knockedOut = true;
		return Event::KnockOut;
	}

	if (!m_knockedOut || !first.IsFull() || !second.IsFull())
		return Event::None;

	m_knockedOut = false;
	return Event::RoundStart;
}

void RoundEvents::Reset()
{
	m_knockedOut = false;
}
