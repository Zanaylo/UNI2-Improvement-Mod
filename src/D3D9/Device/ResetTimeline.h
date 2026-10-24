#pragma once

class ResetTimeline
{
public:
	struct Summary
	{
		double awayMs;
		double activationToResetMs;
		double modReleaseMs;
		double direct3dMs;
		int failedResets;
		double resetToFrameMs;
		double totalMs;
	};

	void Deactivated(double nowMs);
	void Activated(double nowMs);
	void ResetBegan(double nowMs);
	void ModReleased(double nowMs);
	void ResetReturned(double nowMs, bool succeeded);
	bool Presented(double nowMs, Summary& out);

private:
	double m_deactivatedAt = -1.0;
	double m_activatedAt = -1.0;
	double m_firstResetAt = -1.0;
	double m_attemptBegan = 0.0;
	double m_released = 0.0;
	double m_modReleaseMs = 0.0;
	double m_direct3dMs = 0.0;
	double m_resetDoneAt = -1.0;
	int m_failedResets = 0;
};
