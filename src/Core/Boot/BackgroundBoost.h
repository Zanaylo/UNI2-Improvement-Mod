#pragma once

class BackgroundBoost
{
public:
	enum Change
	{
		Change_None,
		Change_Raise,
		Change_Restore
	};

	Change Update(bool enabled, bool focused, bool online);
	bool IsRaised() const;

private:
	bool m_raised = false;
};
