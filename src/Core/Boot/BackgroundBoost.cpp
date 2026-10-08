#include "Core/Boot/BackgroundBoost.h"

BackgroundBoost::Change BackgroundBoost::Update(bool enabled, bool focused, bool online)
{
	const bool wanted = enabled && !focused && online;

	if (wanted == m_raised)
		return Change_None;

	m_raised = wanted;
	return wanted ? Change_Raise : Change_Restore;
}

bool BackgroundBoost::IsRaised() const
{
	return m_raised;
}
