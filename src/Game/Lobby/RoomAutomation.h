#pragma once

#include <cstdint>

namespace RoomAutomation
{
	enum class CreateStage
	{
		Idle,
		Armed,
		EnteringNetwork,
		PickingCreateRoom,
		OpeningPlayerMatch,
		CreatingRoom,
		Failed
	};

	bool Install();

	int ClaimNetworkPad();
	bool RequestScene(uint32_t scene);
	bool EnterNetwork();
	bool LeaveTraining();

	bool CanJoin();
	bool Join(uint64_t lobby, uint64_t owner);

	void StartCreate(int firstTo);
	void SetCreateFirstTo(int firstTo);
	void StopCreate();
	CreateStage StepCreate();
	CreateStage CurrentCreateStage();
	const char* CreateStageText(CreateStage stage);
	bool CreateTimedOut();
}
