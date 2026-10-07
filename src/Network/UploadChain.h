#pragma once

class UploadChain
{
public:
	bool IsIdle() const;

	void ReplaySlotStarted();
	bool ReplaySlotFinished(int result);

	bool ProfileDue(bool battleRunning) const;
	void ProfileStarted();
	void ProfileNotStarted();
	void ProfileFinished();

	bool TakeGameChainHandBack();

private:
	enum class Step
	{
		Idle,
		ReplaySlot,
		ProfileDue,
		Profile
	};

	Step m_step = Step::Idle;
	bool m_holdsGameChain = false;
};
