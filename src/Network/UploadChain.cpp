#include "Network/UploadChain.h"

bool UploadChain::IsIdle() const
{
	return m_step == Step::Idle;
}

void UploadChain::ReplaySlotStarted()
{
	if (m_step != Step::Idle)
		return;

	m_step = Step::ReplaySlot;
	m_holdsGameChain = true;
}

bool UploadChain::ReplaySlotFinished(int result)
{
	if (m_step != Step::ReplaySlot)
		return false;

	m_step = Step::ProfileDue;
	return result == 0;
}

bool UploadChain::ProfileDue(bool battleRunning) const
{
	return m_step == Step::ProfileDue && !battleRunning;
}

void UploadChain::ProfileStarted()
{
	if (m_step != Step::ProfileDue)
		return;

	m_step = Step::Profile;
}

void UploadChain::ProfileNotStarted()
{
	if (m_step != Step::ProfileDue)
		return;

	m_step = Step::Idle;
}

void UploadChain::ProfileFinished()
{
	if (m_step != Step::Profile)
		return;

	m_step = Step::Idle;
}

bool UploadChain::TakeGameChainHandBack()
{
	if (!m_holdsGameChain || !IsIdle())
		return false;

	m_holdsGameChain = false;
	return true;
}
