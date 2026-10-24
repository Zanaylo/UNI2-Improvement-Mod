#include "D3D9/Device/ResetTimeline.h"

namespace {

constexpr double kUnknown = -1.0;

bool Known(double moment)
{
	return moment >= 0.0;
}

}

void ResetTimeline::Deactivated(double nowMs)
{
	m_deactivatedAt = nowMs;
	m_activatedAt = kUnknown;
}

void ResetTimeline::Activated(double nowMs)
{
	m_activatedAt = nowMs;
}

void ResetTimeline::ResetBegan(double nowMs)
{
	if (!Known(m_firstResetAt))
		m_firstResetAt = nowMs;

	m_attemptBegan = nowMs;
	m_released = nowMs;
}

void ResetTimeline::ModReleased(double nowMs)
{
	m_modReleaseMs += nowMs - m_attemptBegan;
	m_released = nowMs;
}

void ResetTimeline::ResetReturned(double nowMs, bool succeeded)
{
	m_direct3dMs += nowMs - m_released;

	if (!succeeded)
	{
		++m_failedResets;
		return;
	}

	m_resetDoneAt = nowMs;
}

bool ResetTimeline::Presented(double nowMs, Summary& out)
{
	if (!Known(m_resetDoneAt))
		return false;

	const bool activated = Known(m_activatedAt);
	const double start = activated ? m_activatedAt : m_firstResetAt;

	out.awayMs = activated && Known(m_deactivatedAt) ? m_activatedAt - m_deactivatedAt : kUnknown;
	out.activationToResetMs = activated ? m_firstResetAt - m_activatedAt : kUnknown;
	out.modReleaseMs = m_modReleaseMs;
	out.direct3dMs = m_direct3dMs;
	out.failedResets = m_failedResets;
	out.resetToFrameMs = nowMs - m_resetDoneAt;
	out.totalMs = nowMs - start;

	*this = ResetTimeline();
	return true;
}
