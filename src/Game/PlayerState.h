// The per-frame move state the game's own scripts see through GetMvStatus, read straight out of
// PLAYER_DATA. Invulnerability has three sources; ask GetInvuln, never one of them directly.

#pragma once

#include <cstdint>

#include "Game/GameOffsets.h"

namespace PlayerState
{
	struct State
	{
		uint16_t pattern;
		uint16_t frameIndex;
		uint16_t frameFlag;
		uint32_t mvCountFrame;
		bool frameUpdate;

		uint8_t mvFlagA;
		uint8_t mvFlagB;
		uint32_t mvValue;

		bool hasContext;
		uint16_t statusWord;
		uint8_t statusA;
		uint8_t statusB;
		uint8_t statusC;
		uint8_t mvStatus;

		int attackBoxes;

		int boxCounts[4];
		uint32_t normalBoxMask;

		bool counterWindow;

		bool atemiWindow;
		int16_t atemiBox;

		uint8_t invulnKind;

		uint32_t attrInvuln;

		uint32_t attackAttrs;

		uint8_t hurtboxCount;
		bool existNoKurai;

		uint8_t counterState;

		uint8_t shieldSuccess;

		uint32_t ownedObjects;
		bool projectileActive;

		bool actionable;

		uint32_t actionableRaw;

		uint32_t actionLock;

		uint32_t actionKind;

		uint32_t moveCodeEx[GameOffsets::kMoveCodeExCount];

		bool hasCancelFlags;
		uint8_t cancelFlagsA;
		uint8_t cancelFlagsB;

		bool cancelFree;

		uint8_t cancelNormal;
		uint8_t cancelSpecial;

		uint32_t command;
		uint8_t running;

		uint8_t airJumpOK;

		uint8_t stance;
		uint8_t stanceOverride;
		int32_t stanceOverrideTime;

		int32_t positionY;
		uint32_t hitstop;
		uint32_t stunTimer;

		bool inReaction;

		uint8_t techWindow;
		uint16_t ukemiTime;

		uint8_t mutekiStrike;
		uint8_t mutekiThrow;

		bool armor;

		uint8_t shield;
		uint8_t vguardTime;
	};

	struct FrameDisplay
	{
		int startup;
		int total;
		int advantage;
	};

	bool ReadFrameDisplay(FrameDisplay& out);

	int ReadFrameDisplayRaw(uint32_t* out, int count);

	constexpr uint8_t kInvulnStrike = 3;
	constexpr uint8_t kInvulnThrow = 4;
	constexpr uint8_t kInvulnBoth = 5;
	constexpr uint8_t kInvulnHighMid = 1;
	constexpr uint8_t kInvulnLowMid = 2;

	enum Invuln : uint16_t
	{
		Invuln_Full = 1u << 0,
		Invuln_Strike = 1u << 1,
		Invuln_Throw = 1u << 2,
		Invuln_Projectile = 1u << 3,
		Invuln_Head = 1u << 4,
		Invuln_Body = 1u << 5,
		Invuln_Legs = 1u << 6,
		Invuln_Dive = 1u << 7,
		Invuln_HighMid = 1u << 8,
		Invuln_LowMid = 1u << 9,

		Invuln_COUNT = 10
	};

	uint16_t GetInvuln(const State& state);

	int ReadCatchBoxIndex(void* playerData);

	inline bool IsStrikeInvulnerable(const State& state)
	{
		const uint16_t invuln = GetInvuln(state);
		return (invuln & (Invuln_Full | Invuln_Strike)) != 0;
	}

	inline bool IsThrowInvulnerable(const State& state)
	{
		const uint16_t invuln = GetInvuln(state);
		return (invuln & (Invuln_Full | Invuln_Throw)) != 0;
	}

	inline bool IsInvulnerable(const State& state)
	{
		return GetInvuln(state) != 0;
	}

	inline uint8_t GetStance(const State& state)
	{
		return state.stanceOverrideTime > 0 ? state.stanceOverride : state.stance;
	}

	inline bool IsAirborne(const State& state)
	{
		return GetStance(state) == GameOffsets::kStatusAir;
	}

	inline bool CanLeaveFrameOnWhiff(const State& state)
	{
		return state.cancelFree ||
			state.cancelNormal == GameOffsets::kCancelAlways ||
			state.cancelSpecial == GameOffsets::kCancelAlways;
	}

	inline uint32_t RemainingStun(const State& state)
	{
		return state.hitstop + state.stunTimer;
	}

	bool Read(void* playerData, State& out);
}
