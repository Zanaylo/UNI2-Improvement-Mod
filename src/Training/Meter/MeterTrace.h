#pragma once

#include "Training/Meter/FrameMeter.h"

#include <cstdint>

struct DiagFrame
{
	bool recorded;

	bool flashing;

	int barIndex;
	uint16_t pattern[FrameMeter::kPlayers];
	uint32_t actionable[FrameMeter::kPlayers];
	uint16_t hitstop[FrameMeter::kPlayers];
	uint16_t stun[FrameMeter::kPlayers];
	int16_t attackBoxes[FrameMeter::kPlayers];
	uint8_t shield[FrameMeter::kPlayers];
	uint8_t state[FrameMeter::kPlayers];
	uint8_t counts[FrameMeter::kPlayers][4];
	uint32_t boxMask[FrameMeter::kPlayers];
	uint8_t invulnKind[FrameMeter::kPlayers];
	uint8_t counterState[FrameMeter::kPlayers];
	uint8_t techWindow[FrameMeter::kPlayers];
	uint16_t ukemiTime[FrameMeter::kPlayers];
	uint8_t shieldSuccess[FrameMeter::kPlayers];
	uint8_t muteki[FrameMeter::kPlayers][2];
	bool projectile[FrameMeter::kPlayers];
	bool teching[FrameMeter::kPlayers];
	bool inReaction[FrameMeter::kPlayers];
	int freeFrom[FrameMeter::kPlayers];
	int moveEndAt[FrameMeter::kPlayers];
	int freeAfterTech[FrameMeter::kPlayers];
	uint32_t actionLock[FrameMeter::kPlayers];
	uint32_t actionKind[FrameMeter::kPlayers];
	uint32_t command[FrameMeter::kPlayers];
	uint32_t mvCountFrame[FrameMeter::kPlayers];
	bool airJumpOK[FrameMeter::kPlayers];
	uint8_t stance[FrameMeter::kPlayers];
	int32_t positionY[FrameMeter::kPlayers];
	uint32_t moveCodeEx1[FrameMeter::kPlayers];
	uint32_t moveCodeEx2[FrameMeter::kPlayers];
	uint32_t moveCodeEx3[FrameMeter::kPlayers];
	uint32_t moveCodeEx7[FrameMeter::kPlayers];
	uint32_t attrInvuln[FrameMeter::kPlayers];
	uint16_t invuln[FrameMeter::kPlayers];
	int16_t atemiBox[FrameMeter::kPlayers];
	bool cancelFree[FrameMeter::kPlayers];
	uint8_t cancelNormal[FrameMeter::kPlayers];
	uint8_t cancelSpecial[FrameMeter::kPlayers];
	uint8_t hurtboxes[FrameMeter::kPlayers];
};

namespace MeterTrace
{
	void Queue(const DiagFrame* frames, int length);
	void Shutdown();
}
