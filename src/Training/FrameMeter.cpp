#include "Training/FrameMeter.h"

#include "Core/Profiler.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/CharaTracker.h"
#include "Game/GameOffsets.h"
#include "Game/CharaTables.h"
#include "Game/FrameDataTable.h"
#include "Game/GameTables.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Game/PlayerState.h"
#include "Training/FrameStepper.h"
#include "Training/SuperFlash.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int kEntityRefreshFrames = 30;

constexpr int kIdleFramesToEnd = 24;

constexpr int kIdlePadding = 3;

constexpr uint16_t kGuardPatternStand = 17;
constexpr uint16_t kGuardPatternCrouch = 18;
constexpr uint16_t kGuardPatternAir = 19;
constexpr uint16_t kHitReactionFirstPattern = 300;

bool IsGuardPattern(uint16_t pattern)
{
	return pattern == kGuardPatternStand || pattern == kGuardPatternCrouch ||
		pattern == kGuardPatternAir;
}

constexpr int kBlockConfirmFrames = 3;

struct Tracked
{
	void* entity;
	uint16_t pattern;
	bool sawActive;

	bool airAttackLanding;

	bool sawParry;

	bool inMove;

	uint32_t lastMvCountFrame;
	bool hasMvCountFrame;

	bool wasAirJumpOK;
	int landingSlack;

	uint16_t stunPattern;
	bool hasStunPattern;
	bool stunReleased;

	int lockFreeAt = -1;
	int lockFreeFrame = -1;
	bool lockWasHeld = false;

	uint16_t lockPattern = 0;

	bool canAct;

	bool heldRun;
	FrameMeter::State heldLabel;

	FrameMeter::State lastState;

	uint32_t lastCommand;

	int charaNumber = -1;

	uint32_t lastHitstop;
	int comboCount;
	int blockedRun;
	int blockedTotal;
	int reportedCombo;
	int reportedBlocked;

	int pendingBlockFrames;
	int pendingAttacker;

	FrameMeter::State airKind;

	bool heldPending;
};

struct MoveSpan
{
	bool valid;
	int startup;
	int active;
	int recovery;
	int total;
	bool hadActive;
};

FrameMeter::Frame g_frames[FrameMeter::kPlayers][FrameMeter::kCapacity] = {};
int g_length = 0;

SuperFlash g_flash;

int g_frame = 0;
bool g_recording = false;
int g_idleRun = 0;

Tracked g_tracked[FrameMeter::kPlayers] = {};
int g_trackedCount = 0;
int g_framesUntilRefresh = 0;
uint32_t g_lastBattleFrame = 0;
bool g_hasBattleFrame = false;
bool g_restartPending = false;

bool g_hadAttackBoxes[FrameMeter::kPlayers] = {};
int g_returnedAt[FrameMeter::kPlayers] = {};
int g_stunEndedAt[FrameMeter::kPlayers] = {};

int g_freeFrom[FrameMeter::kPlayers] = {};
uint16_t g_freePattern[FrameMeter::kPlayers] = {};
int g_moveEndAt[FrameMeter::kPlayers] = {};

int g_freeFromFrame[FrameMeter::kPlayers] = {};
int g_moveEndFrame[FrameMeter::kPlayers] = {};
int g_lockEndFrame[FrameMeter::kPlayers] = {};
int g_downEndFrame[FrameMeter::kPlayers] = {};
int g_downTakenFrame[FrameMeter::kPlayers] = {};
int g_techEndedFrame[FrameMeter::kPlayers] = {};
int g_returnedFrame[FrameMeter::kPlayers] = {};

int g_heldUntilFrame[FrameMeter::kPlayers] = {};
int g_heldUntilBar[FrameMeter::kPlayers] = {};
int g_techStartedFrame[FrameMeter::kPlayers] = {};

int g_cancelFreeFrame[FrameMeter::kPlayers] = {};
uint8_t g_cancelFlagsAtFree[FrameMeter::kPlayers] = {};
bool g_sawCancelFlags[FrameMeter::kPlayers] = {};

bool g_lockHeldAtFree[FrameMeter::kPlayers] = {};
int g_lockEndAt[FrameMeter::kPlayers] = {};

int g_downEndAt[FrameMeter::kPlayers] = {};
int g_downTakenAt[FrameMeter::kPlayers] = {};
bool g_wasTrade = false;

int g_techStartedAt[FrameMeter::kPlayers] = {};
bool g_techSeen[FrameMeter::kPlayers] = {};

int g_techEndedAt[FrameMeter::kPlayers] = {};
int g_advantageAfterRecovery = 0;
bool g_hasAfterRecovery = false;

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

DiagFrame g_diag[FrameMeter::kCapacity] = {};
int g_diagLength = 0;

bool g_diagEnabled = false;

DiagFrame g_tracePending[FrameMeter::kCapacity] = {};
int g_tracePendingLength = 0;

HANDLE g_traceThread = nullptr;
HANDLE g_traceSignal = nullptr;
volatile LONG g_traceBusy = 0;
volatile LONG g_traceStop = 0;

bool InStunPattern(uint16_t pattern)
{
	return pattern >= kHitReactionFirstPattern || IsGuardPattern(pattern);
}

void UpdateCanAct(Tracked& tracked, const PlayerState::State& state)
{
	if (state.stunTimer > 0)
	{
		tracked.stunPattern = state.pattern;
		tracked.hasStunPattern = true;
		tracked.stunReleased = false;
	}
	else if (tracked.hasStunPattern && state.pattern == tracked.stunPattern)
	{
		tracked.stunReleased = true;
	}
	else
	{
		tracked.hasStunPattern = false;
		tracked.stunReleased = false;
	}

	if (state.actionLock != 0)
	{
		tracked.lockFreeAt = -1;
		tracked.lockFreeFrame = -1;
		tracked.lockWasHeld = true;
		tracked.lockPattern = state.pattern;
	}
	else if (tracked.lockWasHeld)
	{

		const bool startedSomethingElse = state.pattern != tracked.lockPattern &&
			state.actionKind != 0;

		if (!startedSomethingElse)
		{
			if (tracked.lockFreeAt < 0)
			{
				tracked.lockFreeAt = g_length;
				tracked.lockFreeFrame = g_frame;
			}
		}
		else
		{
			tracked.lockWasHeld = false;
			tracked.lockFreeAt = -1;
			tracked.lockFreeFrame = -1;
		}
	}

	tracked.canAct = state.actionable || (tracked.stunReleased && state.hitstop == 0);
}

FrameMeter::AutoPauseConfig g_autoPause = {};

bool g_hasAdvantage = false;
int g_advantage = 0;
int g_advantageAttacker = -1;

MoveSpan g_lastMove[FrameMeter::kPlayers] = {};

bool BattleFrameAdvanced()
{
	uint32_t counter = 0;
	if (!TryReadMemory(&counter, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kFrameCounterA)),
		sizeof(counter)))
	{
		return false;
	}

	if (g_hasBattleFrame && counter == g_lastBattleFrame)
		return false;

	if (g_hasBattleFrame && counter < g_lastBattleFrame)
	{
		g_restartPending = true;
		FrameMeter::Reset();
		g_hasBattleFrame = true;
		g_lastBattleFrame = counter;
		return false;
	}

	g_hasBattleFrame = true;
	g_lastBattleFrame = counter;
	return true;
}

int ReadCharaNumber(void* entity)
{
	uint32_t side = 0;
	if (!MemoryMap::ReadStructDword(entity, GameOffsets::kCharaSideIndex, side))
		return -1;

	side &= 0xff;
	if (side > 1)
		return -1;

	uint32_t number = 0;
	if (!MemoryMap::ReadDwordAt(RvaToAddress(GameOffsets::kCharaRecordBase) +
		side * GameOffsets::kCharaRecordStride, number))
	{
		return -1;
	}

	return static_cast<int>(number);
}

void RefreshEntities()
{
	void* candidates[16] = {};
	int found = 0;

	CharaTracker::Entry entry = {};
	for (int i = 0; i < CharaTracker::GetEntryCount(); ++i)
	{
		if (!CharaTracker::GetEntry(i, entry))
			continue;

		if (!MemoryMap::IsPlayerData(entry.charaData))
			continue;

		found = MemoryMap::EnumeratePlayerData(entry.charaData, candidates, 16);
		if (found > 0)
			break;
	}

	int kept = 0;
	for (int i = 0; i < found && kept < FrameMeter::kPlayers; ++i)
	{
		uint32_t context = 0;
		if (!MemoryMap::ReadStructDword(candidates[i], GameOffsets::kCharaFrameObject, context))
			continue;

		if (context == 0)
			continue;

		if (g_tracked[kept].entity != candidates[i])
		{
			g_tracked[kept] = Tracked();
			g_tracked[kept].entity = candidates[i];
		}

		g_tracked[kept].charaNumber = ReadCharaNumber(candidates[i]);

		++kept;
	}

	g_trackedCount = kept;
}

FrameMeter::State MovementKind(uint32_t command)
{
	switch (command)
	{
	case GameOffsets::kCommandDashForward:
	case GameOffsets::kCommandDashBack:
		return FrameMeter::State::Movement;
	case GameOffsets::kCommandAssaultAir:
	case GameOffsets::kCommandAssaultGround:
		return FrameMeter::State::AirMovement;
	default:
		break;
	}

	if (command >= GameOffsets::kCommandJumpFirst && command <= GameOffsets::kCommandJumpLast)
		return FrameMeter::State::Jump;

	return FrameMeter::State::None;
}

constexpr int kNoChara = -2;
constexpr int kAuthoredMemoSlots = 16;

struct AuthoredMemo
{
	int chara;
	int pattern;
	bool found;
	FrameDataTable::Pattern value;
};

AuthoredMemo g_authoredMemo[kAuthoredMemoSlots] = {};
bool g_authoredMemoPrimed = false;

bool ReadAuthored(int chara, uint16_t pattern, FrameDataTable::Pattern& out)
{
	if (chara < 0)
		return false;

	if (!g_authoredMemoPrimed)
	{
		for (AuthoredMemo& entry : g_authoredMemo)
			entry.chara = kNoChara;

		g_authoredMemoPrimed = true;
	}

	AuthoredMemo& slot = g_authoredMemo[(chara * 31 + pattern) & (kAuthoredMemoSlots - 1)];

	if (slot.chara != chara || slot.pattern != pattern)
	{
		slot.chara = chara;
		slot.pattern = pattern;
		slot.value = FrameDataTable::Pattern();
		slot.found = FrameDataTable::Get(chara, pattern, slot.value);
	}

	out = slot.value;
	return slot.found;
}

constexpr int kNamedMemoSlots = 16;

struct NamedMemo
{
	int chara;
	uint32_t command;
	bool named;
};

NamedMemo g_namedMemo[kNamedMemoSlots] = {};
bool g_namedMemoPrimed = false;

bool LookupNamed(int chara, uint32_t command)
{
	return CharaTables::MoveName(chara, command)[0] != '\0' ||
		GameTables::CommandName(command)[0] != '\0';
}

bool CommandIsNamed(int chara, uint32_t command)
{
	if (!g_namedMemoPrimed)
	{
		for (NamedMemo& entry : g_namedMemo)
			entry.chara = kNoChara;

		g_namedMemoPrimed = true;
	}

	NamedMemo& slot = g_namedMemo[(chara * 31 + command) & (kNamedMemoSlots - 1)];

	if (slot.chara != chara || slot.command != command)
	{
		slot.chara = chara;
		slot.command = command;
		slot.named = LookupNamed(chara, command);
	}

	return slot.named;
}

bool PatternHasNoAttack(int chara, uint16_t pattern)
{
	FrameDataTable::Pattern authored = {};
	if (!ReadAuthored(chara, pattern, authored))
		return false;

	return authored.active == 0;
}

bool PatternHasAttack(int chara, uint16_t pattern)
{
	FrameDataTable::Pattern authored = {};
	if (!ReadAuthored(chara, pattern, authored))
		return false;

	return authored.active > 0;
}

FrameMeter::State Movement(Tracked& tracked, const PlayerState::State& state)
{

	if ((state.moveCodeEx[1] & GameOffsets::kMoveCode1Jump) != 0 && !tracked.sawActive &&
		!tracked.airAttackLanding)
	{
		tracked.airKind = FrameMeter::State::Jump;
		return FrameMeter::State::Jump;
	}

	if ((state.actionKind & GameOffsets::kActionKindAnyMove) != 0 || tracked.sawActive ||
		tracked.airAttackLanding)
	{
		return FrameMeter::State::None;
	}

	if ((state.moveCodeEx[2] & GameOffsets::kMoveCode2FromAssault) != 0)
		return FrameMeter::State::None;

	const FrameMeter::State kind = MovementKind(state.command);

	if (kind != FrameMeter::State::None)
	{

		if (kind != FrameMeter::State::Movement || state.airJumpOK != 0)
			tracked.airKind = kind;

		return kind;
	}

	if (state.running != 0)
		return FrameMeter::State::Movement;

	if (tracked.airKind != FrameMeter::State::None && (state.airJumpOK != 0 || !tracked.canAct))
		return tracked.airKind;

	return FrameMeter::State::None;
}

bool ContinuesAMove(int player)
{
	if (g_length <= 0)
		return false;

	const FrameMeter::State previous = g_frames[player][g_length - 1].state;
	return previous == FrameMeter::State::Active || previous == FrameMeter::State::Recovery ||
		previous == FrameMeter::State::Cancellable;
}

constexpr int kLandingSlackFrames = 2;

uint16_t DrawnInvuln(const PlayerState::State& state, FrameMeter::State classified)
{
	uint16_t invuln = PlayerState::GetInvuln(state);

	const bool held = classified == FrameMeter::State::Hitstun ||
		classified == FrameMeter::State::Blockstun;

	if (PlayerState::IsAirborne(state) && !held && (invuln & PlayerState::Invuln_Full) == 0)
		invuln |= PlayerState::Invuln_Throw;

	if (held)
		invuln &= ~static_cast<uint16_t>(PlayerState::Invuln_Throw);

	return invuln;
}

FrameMeter::State Classify(int player, const PlayerState::State& state)
{
	Tracked& tracked = g_tracked[player];

	UpdateCanAct(tracked, state);

	if (state.pattern != tracked.pattern)
	{
		tracked.pattern = state.pattern;
		tracked.sawActive = false;
		tracked.sawParry = false;
	}

	if (tracked.wasAirJumpOK && state.airJumpOK == 0)
		tracked.landingSlack = kLandingSlackFrames;
	else if (tracked.landingSlack > 0)
		--tracked.landingSlack;

	tracked.wasAirJumpOK = state.airJumpOK != 0;

	const bool newMove = tracked.hasMvCountFrame && tracked.landingSlack == 0 &&
		state.mvCountFrame < tracked.lastMvCountFrame;

	tracked.lastMvCountFrame = state.mvCountFrame;
	tracked.hasMvCountFrame = true;

	if (newMove)
	{
		tracked.sawActive = false;
		tracked.sawParry = false;
		tracked.airAttackLanding = false;
		tracked.inMove = false;
	}

	const bool held = !tracked.canAct &&
		(state.stunTimer > 0 || state.inReaction || tracked.stunReleased);

	if (!held)
		tracked.heldRun = false;

	if (held)
	{
		tracked.airKind = FrameMeter::State::None;
		tracked.heldPending = true;
	}
	else if (tracked.canAct && state.airJumpOK == 0)
	{
		tracked.airKind = FrameMeter::State::None;
	}

	if (tracked.canAct)
		tracked.heldPending = false;

	const bool trulyFree = tracked.canAct && state.cancelFree;

	if (held || trulyFree)
		tracked.inMove = false;

	if ((state.counterWindow && !state.inReaction) || state.shield != 0)
	{
		tracked.sawParry = true;
		tracked.inMove = true;
		return FrameMeter::State::Parry;
	}

	if (!held && state.command == GameOffsets::kCommandForwardShift)
		return FrameMeter::State::Dodge;

	if (held || trulyFree)
		tracked.airAttackLanding = false;

	if (state.attackBoxes > 0)
	{
		tracked.sawActive = true;
		tracked.inMove = true;

		if (state.airJumpOK != 0)
			tracked.airAttackLanding = true;

		return FrameMeter::State::Active;
	}

	if (held)
	{

		if (!tracked.heldRun)
		{
			tracked.heldRun = true;
			tracked.heldLabel = state.pattern >= kHitReactionFirstPattern
				? FrameMeter::State::Hitstun : FrameMeter::State::Blockstun;
		}

		return tracked.heldLabel;
	}

	if (IsGuardPattern(state.pattern) && !tracked.canAct)
		return FrameMeter::State::Blockstun;

	const FrameMeter::State movement = Movement(tracked, state);
	if (movement != FrameMeter::State::None)
		return movement;

	if (tracked.canAct && !state.cancelFree && tracked.inMove)
	{

		return PlayerState::CanLeaveFrameOnWhiff(state)
			? FrameMeter::State::Cancellable : FrameMeter::State::Recovery;
	}

	const bool startingOwnMove = (state.actionKind & GameOffsets::kActionKindAnyMove) != 0 &&
		!tracked.sawActive && !tracked.inMove &&
		PatternHasAttack(tracked.charaNumber, state.pattern);

	if (tracked.canAct && !startingOwnMove)
		return FrameMeter::State::Idle;

	if (state.actionKind == 0 && tracked.heldPending)
		return FrameMeter::State::Hitstun;

	if (tracked.sawActive || tracked.sawParry || tracked.airAttackLanding)
	{
		return PlayerState::CanLeaveFrameOnWhiff(state)
			? FrameMeter::State::Cancellable : FrameMeter::State::Recovery;
	}

	if (ContinuesAMove(player) && PatternHasNoAttack(tracked.charaNumber, state.pattern))
	{
		return PlayerState::CanLeaveFrameOnWhiff(state)
			? FrameMeter::State::Cancellable : FrameMeter::State::Recovery;
	}

	return state.armor ? FrameMeter::State::Armor : FrameMeter::State::Startup;
}

bool IsMoveCell(FrameMeter::State state)
{
	return state == FrameMeter::State::Startup || state == FrameMeter::State::Armor ||
		state == FrameMeter::State::Active || state == FrameMeter::State::Recovery ||
		state == FrameMeter::State::Parry || state == FrameMeter::State::Cancellable;
}

void UpdateLastMoveFromBar(int player)
{
	int end = -1;

	for (int i = g_length - 1; i >= 0; --i)
	{
		if (IsMoveCell(g_frames[player][i].state))
		{
			end = i;
			break;
		}
	}

	if (end < 0)
		return;

	int begin = end;
	while (begin > 0 && IsMoveCell(g_frames[player][begin - 1].state))
		--begin;

	const uint16_t movePattern = g_frames[player][begin].pattern;

	int last = end;
	while (last + 1 < g_length && g_frames[player][last + 1].state == FrameMeter::State::Idle &&
		(g_frames[player][last + 1].pattern == movePattern || g_frames[player][last + 1].airJumpOK))
	{
		++last;
	}

	int active = 0;
	int startup = 0;
	bool sawActive = false;

	int parryAt = -1;

	for (int i = begin; i <= end; ++i)
	{
		if (g_frames[player][i].state == FrameMeter::State::Parry && parryAt < 0)
			parryAt = i - begin + 1;

		if (g_frames[player][i].state != FrameMeter::State::Active)
			continue;

		if (!sawActive)
		{

			sawActive = true;
			startup = i - begin + 1;
		}

		++active;
	}

	if (!sawActive && parryAt >= 0)
		startup = parryAt;

	const int total = last - begin + 1;

	g_lastMove[player].valid = true;
	g_lastMove[player].startup = (sawActive || parryAt >= 0) ? startup : total;
	g_lastMove[player].active = active;
	g_lastMove[player].recovery = total - g_lastMove[player].startup - active;
	g_lastMove[player].total = total;
	g_lastMove[player].hadActive = sawActive;
}

void DropOldestFrames(int count)
{
	if (count <= 0 || count > g_length)
		return;

	const int remaining = g_length - count;

	for (int p = 0; p < FrameMeter::kPlayers; ++p)
	{
		memmove(g_frames[p], g_frames[p] + count,
			sizeof(FrameMeter::Frame) * static_cast<size_t>(remaining));

		int* const indices[] = {
			&g_returnedAt[p], &g_freeFrom[p], &g_moveEndAt[p], &g_stunEndedAt[p],
			&g_techStartedAt[p], &g_techEndedAt[p], &g_lockEndAt[p], &g_downEndAt[p],
			&g_downTakenAt[p]
		};

		for (int i = 0; i < static_cast<int>(sizeof(indices) / sizeof(indices[0])); ++i)
		{
			int& index = *indices[i];
			if (index < 0)
				continue;

			index = index >= count ? index - count : -1;
		}
	}

	g_length = remaining;
}

bool DelayWanted(FrameMeter::PauseSource source)
{
	if (!g_autoPause.resumeDelay)
		return false;

	switch (source)
	{
	case FrameMeter::PauseSource::MoveStarts:   return g_autoPause.delayOnMoveStarts;
	case FrameMeter::PauseSource::HitLands:     return g_autoPause.delayOnHitLands;
	case FrameMeter::PauseSource::ComboReaches: return g_autoPause.delayOnComboReaches;
	case FrameMeter::PauseSource::BlockReaches: return g_autoPause.delayOnBlockReaches;
	default:                                    return false;
	}
}

void ArmedPause(FrameMeter::PauseSource source)
{
	FrameStepper::SetPausedAutomatically(DelayWanted(source) ? g_autoPause.resumeDelayFrames : 0);
}

bool AutoPauseWatches(int player)
{
	if (player < 0 || player >= FrameMeter::kPlayers)
		return false;

	return g_autoPause.player[player];
}

bool AutoPauseTriggers(FrameMeter::State state)
{
	return g_autoPause.onMoveStarts && state == FrameMeter::State::Startup;
}

void CheckAutoPause(int player, FrameMeter::State state)
{
	if (!AutoPauseWatches(player) || FrameStepper::IsPaused())
		return;

	if (!AutoPauseTriggers(state))
		return;

	if (AutoPauseTriggers(g_tracked[player].lastState))
		return;

	ArmedPause(FrameMeter::PauseSource::MoveStarts);
}

int ReadComboCount(void* entity)
{
	uint32_t side = 0;
	if (!MemoryMap::ReadStructDword(entity, GameOffsets::kCharaSideIndex, side))
		return 0;

	side &= 0xff;
	if (side > 1)
		return 0;

	const uintptr_t record = RvaToAddress(GameOffsets::kComboRecordBase) +
		side * GameOffsets::kComboRecordStride;

	uint32_t valid = 0;
	if (!MemoryMap::ReadDwordAt(record + GameOffsets::kComboRecordValid, valid) || valid == 0)
		return 0;

	uint32_t hits = 0;
	if (!MemoryMap::ReadDwordAt(record + GameOffsets::kComboHitCount, hits))
		return 0;

	return static_cast<int>(hits);
}

void UpdateCounters(const PlayerState::State* states, const bool* valid,
	const FrameMeter::State* classified, int count)
{
	bool comboAdvanced[FrameMeter::kPlayers] = {};

	for (int p = 0; p < FrameMeter::kPlayers && p < count; ++p)
	{
		if (!valid[p])
			continue;

		Tracked& tracked = g_tracked[p];
		const int combo = ReadComboCount(tracked.entity);

		comboAdvanced[p] = combo > tracked.comboCount;
		tracked.comboCount = combo;
	}

	for (int p = 0; p < FrameMeter::kPlayers && p < count; ++p)
	{
		if (!valid[p])
			continue;

		Tracked& tracked = g_tracked[p];
		const uint32_t previousHitstop = tracked.lastHitstop;
		tracked.lastHitstop = states[p].hitstop;

		const int other = p == 0 ? 1 : 0;
		const bool otherValid = other < count && valid[other];

		if (comboAdvanced[p] && g_autoPause.onHit && AutoPauseWatches(p) &&
			!FrameStepper::IsPaused())
		{
			ArmedPause(FrameMeter::PauseSource::HitLands);
		}

		if (states[p].actionable)
			tracked.blockedRun = 0;

		if (tracked.pendingBlockFrames > 0)
		{
			const int attacker = tracked.pendingAttacker;
			const bool scored = attacker >= 0 && attacker < count && comboAdvanced[attacker];

			if (scored)
			{
				tracked.pendingBlockFrames = 0;
			}
			else if (--tracked.pendingBlockFrames == 0)
			{
				++tracked.blockedRun;
				++tracked.blockedTotal;
			}
		}

		if (states[p].hitstop == 0 || previousHitstop != 0)
			continue;

		if (classified[p] == FrameMeter::State::Active)
			continue;

		if (!otherValid)
			continue;

		if (comboAdvanced[other])
			continue;

		tracked.pendingBlockFrames = kBlockConfirmFrames;
		tracked.pendingAttacker = other;
	}
}

void CheckCountAutoPause(int player)
{
	Tracked& tracked = g_tracked[player];
	const int blocked = g_autoPause.blockedAllowsGaps ? tracked.blockedTotal : tracked.blockedRun;

	const bool armed = AutoPauseWatches(player) && !FrameStepper::IsPaused();

	if (armed && g_autoPause.onComboHits && tracked.comboCount > tracked.reportedCombo)
	{
		for (int i = 0; i < FrameMeter::kComboStops; ++i)
		{
			if (g_autoPause.comboStop[i] == tracked.comboCount)
			{
				ArmedPause(FrameMeter::PauseSource::ComboReaches);
				break;
			}
		}
	}

	if (armed && g_autoPause.onBlockedHits && blocked > tracked.reportedBlocked)
	{
		for (int i = 0; i < FrameMeter::kComboStops; ++i)
		{
			if (g_autoPause.blockStop[i] == blocked)
			{
				ArmedPause(FrameMeter::PauseSource::BlockReaches);
				break;
			}
		}
	}

	tracked.reportedCombo = tracked.comboCount;
	tracked.reportedBlocked = blocked;
}

void BeginExchange()
{
	g_recording = true;
	g_length = 0;
	g_frame = 0;
	g_idleRun = 0;
	g_hasAdvantage = false;
	g_hasAfterRecovery = false;
	g_wasTrade = false;
	g_diagLength = 0;

	for (int p = 0; p < FrameMeter::kPlayers; ++p)
	{
		g_hadAttackBoxes[p] = false;
		g_returnedAt[p] = -1;
		g_freeFrom[p] = -1;
		g_moveEndAt[p] = -1;
		g_lockHeldAtFree[p] = false;
		g_lockEndAt[p] = -1;
		g_downEndAt[p] = -1;
		g_downTakenAt[p] = -1;
		g_freeFromFrame[p] = -1;
		g_moveEndFrame[p] = -1;
		g_lockEndFrame[p] = -1;
		g_downEndFrame[p] = -1;
		g_downTakenFrame[p] = -1;
		g_techEndedFrame[p] = -1;
		g_returnedFrame[p] = -1;
		g_techStartedAt[p] = -1;
		g_techStartedFrame[p] = -1;
		g_techEndedAt[p] = -1;
		g_techSeen[p] = false;

		g_heldUntilFrame[p] = g_frame - 1;
		g_heldUntilBar[p] = -1;
		g_cancelFreeFrame[p] = g_frame - 1;
		g_cancelFlagsAtFree[p] = 0;
		g_sawCancelFlags[p] = false;
		g_stunEndedAt[p] = -1;
		g_tracked[p].blockedRun = 0;
		g_tracked[p].blockedTotal = 0;
		g_tracked[p].pendingBlockFrames = 0;
		g_tracked[p].reportedBlocked = 0;
	}
}

void RepaintFree(int player, int from)
{
	if (player < 0 || player >= FrameMeter::kPlayers || from < 0)
		return;

	for (int i = from; i < g_length; ++i)
	{
		if (g_frames[player][i].state != FrameMeter::State::Recovery)
			break;

		g_frames[player][i].state = FrameMeter::State::Idle;
	}
}

void ComputeAdvantage()
{
	if (g_trackedCount != 2)
		return;

	int attackers = 0;
	int soleAttacker = -1;
	for (int p = 0; p < FrameMeter::kPlayers; ++p)
	{
		if (!g_hadAttackBoxes[p])
			continue;

		++attackers;
		soleAttacker = p;
	}

	if (attackers == 0)
		return;

	if (g_returnedAt[0] < 0 || g_returnedAt[1] < 0)
		return;

	if (g_returnedFrame[0] < 0 || g_returnedFrame[1] < 0)
		return;

	g_advantage = g_returnedFrame[1] - g_returnedFrame[0];
	g_advantageAttacker = attackers == 1 ? soleAttacker : -1;
	g_wasTrade = attackers > 1;
	g_hasAdvantage = true;

	if (!g_techSeen[0] && !g_techSeen[1])
		return;

	const int end0 = g_techEndedFrame[0] >= 0 ? g_techEndedFrame[0] : g_returnedFrame[0];
	const int end1 = g_techEndedFrame[1] >= 0 ? g_techEndedFrame[1] : g_returnedFrame[1];

	const int afterTech = end1 - end0;

	const int beforeTech = g_returnedFrame[1] - g_returnedFrame[0];

	if (afterTech == beforeTech)
		return;

	g_advantageAfterRecovery = beforeTech;
	g_advantage = afterTech;
	g_hasAfterRecovery = true;
}

void WriteTraceCsv(const DiagFrame* frames, int length)
{
	if (length == 0)
		return;

	static int take = 0;

	char name[96] = {};
	sprintf_s(name, "UNI2_IM_meter_%s_%02d.csv", GetLogSessionStamp().c_str(), ++take);

	const std::string path = GetModLogPath(name);

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "w") != 0 || file == nullptr)
	{
		LOG("meter trace: could not open %s", path.c_str());
		return;
	}

	fprintf(file, "frame,bar,recorded,flash,player,pattern,state,actionable,hitstop,stun,"
		"attackBoxes,boxEtc,boxKas,boxNorm,projectile,teching,inReaction,invulnKind,mutekiS,mutekiT,"
		"actionLock,moveCode,command,freeFrom,moveEndAt,moveCodeEx1,moveCodeEx2,moveCodeEx3,moveCodeEx7,"
		"attrInvuln,invuln,atemiBox,hurtboxes,cancelFree,cancelN,cancelS,mvCount,"
		"stance,airJumpOK,posY\n");

	for (int i = 0; i < length; ++i)
	{
		const DiagFrame& d = frames[i];

		for (int p = 0; p < FrameMeter::kPlayers; ++p)
		{
			fprintf(file, "%d,%d,%d,%d,%d,%u,%s,%u,%u,%u,%d,%u,%u,%u,%d,%d,%d,%u,%u,%u,0x%x,0x%x,0x%x,%d,%d,"
				"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,%d,%u,%d,%u,%u,%u,%u,%d,%d\n",
				i, d.barIndex, d.recorded ? 1 : 0, d.flashing ? 1 : 0, p, d.pattern[p],
				FrameMeter::GetStateName(static_cast<FrameMeter::State>(d.state[p])),
				d.actionable[p], d.hitstop[p], d.stun[p], d.attackBoxes[p],
				d.counts[p][0], d.counts[p][1], d.counts[p][2],
				d.projectile[p] ? 1 : 0, d.teching[p] ? 1 : 0, d.inReaction[p] ? 1 : 0,
				d.invulnKind[p], d.muteki[p][0], d.muteki[p][1], d.actionLock[p], d.actionKind[p],
				d.command[p], d.freeFrom[p], d.moveEndAt[p],
				d.moveCodeEx1[p], d.moveCodeEx2[p], d.moveCodeEx3[p], d.moveCodeEx7[p],
				d.attrInvuln[p],
				d.invuln[p], d.atemiBox[p], d.hurtboxes[p], d.cancelFree[p] ? 1 : 0,
				d.cancelNormal[p], d.cancelSpecial[p], d.mvCountFrame[p],
				d.stance[p], d.airJumpOK[p] ? 1 : 0, d.positionY[p]);
		}
	}

	fclose(file);
	LOG("meter trace: wrote %s", path.c_str());
}

DWORD WINAPI TraceWriterThread(LPVOID)
{
	while (WaitForSingleObject(g_traceSignal, INFINITE) == WAIT_OBJECT_0)
	{
		if (InterlockedCompareExchange(&g_traceStop, 0, 0) != 0)
			break;

		WriteTraceCsv(g_tracePending, g_tracePendingLength);
		InterlockedExchange(&g_traceBusy, 0);
	}

	return 0;
}

bool EnsureTraceWriter()
{
	if (g_traceThread != nullptr)
		return true;

	g_traceSignal = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	if (g_traceSignal == nullptr)
		return false;

	g_traceThread = CreateThread(nullptr, 0, TraceWriterThread, nullptr, 0, nullptr);
	if (g_traceThread == nullptr)
	{
		CloseHandle(g_traceSignal);
		g_traceSignal = nullptr;
		return false;
	}

	return true;
}

void QueueTraceCsv()
{
	if (!EnsureTraceWriter())
		return;

	if (InterlockedCompareExchange(&g_traceBusy, 1, 0) != 0)
	{
		LOG("meter trace: writer still busy, exchange dropped");
		return;
	}

	memcpy(g_tracePending, g_diag, sizeof(DiagFrame) * static_cast<size_t>(g_diagLength));
	g_tracePendingLength = g_diagLength;

	SetEvent(g_traceSignal);
}

void LogTrace()
{
	if (!g_diagEnabled || g_diagLength == 0)
		return;

	QueueTraceCsv();

	PlayerState::FrameDisplay display = {};
	const bool hasDisplay = PlayerState::ReadFrameDisplay(display);

	LOG_SECTION("meter: exchange trace");
	LOG_RAW("game frame info: startup %d  total %d  advantage %+d%s",
		hasDisplay ? display.startup : 0, hasDisplay ? display.total : 0,
		hasDisplay ? display.advantage : 0, hasDisplay ? "" : "  <unreadable>");

	uint32_t raw[10] = {};
	const int rawCount = PlayerState::ReadFrameDisplayRaw(raw, 10);
	for (int i = 0; i < rawCount; ++i)
		LOG_RAW("  display +0x%02x = %d (0x%08x)", 0x10 + i * 4, static_cast<int>(raw[i]), raw[i]);
	LOG_RAW("meter: advantage %+d%s   p0 free@%d  p1 free@%d   (bar indices)",
		g_advantage, g_wasTrade ? " (trade)" : "", g_returnedAt[0], g_returnedAt[1]);
	LOG_RAW("flash: p0 %d  p1 %d   (frames taken out of the bar)",
		g_flash.GetFrames(0), g_flash.GetFrames(1));
	LOG_RAW("held until: p0 frame %d bar %d   p1 frame %d bar %d   (recorded, not used)",
		g_heldUntilFrame[0], g_heldUntilBar[0], g_heldUntilFrame[1], g_heldUntilBar[1]);
	LOG_RAW("bracket %s %+d   techSeen %d/%d   techStartedAt %d/%d",
		g_hasAfterRecovery ? "shown" : "hidden", g_advantageAfterRecovery,
		g_techSeen[0] ? 1 : 0, g_techSeen[1] ? 1 : 0,
		g_techStartedAt[0], g_techStartedAt[1]);
	LOG_RAW("");
	LOG_RAW("columns: pat act stop stun | boxes etc/kas/norm/atk  normalIdx  ivk cnt shS mtk");

	for (int p = 0; p < FrameMeter::kPlayers; ++p)
	{
		LOG_RAW("");
		LOG_RAW("--- p%d ---", p);
		LOG_RAW("  f  bar#  rec   pat act stop stun | %2s/%2s/%2s/%2s  normalIdx           ivk cnt shS mkS mkT prj tec ukm tch rea  free  end after      lock     kind state",
			"e", "k", "n", "a");

		for (int i = 0; i < g_diagLength; ++i)
		{
			const DiagFrame& d = g_diag[i];

			char mask[33] = {};
			for (int b = 0; b < 32; ++b)
				mask[b] = (d.boxMask[p] & (1u << b)) != 0 ? '1' : '.';

			LOG_RAW("%3d %4d %5s %5u %3u %4u %4u | %2u/%2u/%2u/%2u  %s  %3u %3u %3u %3u %3u %3d %3u %3u %3d %3d %5d %5d %5d %8x %8x %s",
				i, d.barIndex, d.flashing ? "FLSH" : (d.recorded ? "rec" : "STOP"),
				d.pattern[p], d.actionable[p], d.hitstop[p], d.stun[p],
				d.counts[p][0], d.counts[p][1], d.counts[p][2], d.counts[p][3],
				mask, d.invulnKind[p], d.counterState[p], d.shieldSuccess[p],
				d.muteki[p][0], d.muteki[p][1],
				d.projectile[p] ? 1 : 0, d.techWindow[p], d.ukemiTime[p], d.teching[p] ? 1 : 0,
				d.inReaction[p] ? 1 : 0, d.freeFrom[p], d.moveEndAt[p], d.freeAfterTech[p],
				d.actionLock[p], d.actionKind[p],
				FrameMeter::GetStateName(static_cast<FrameMeter::State>(d.state[p])));
		}
	}
}

int g_oracleChecks = 0;
int g_oracleMismatches = 0;

void LogOracle()
{
	PlayerState::FrameDisplay display = {};
	if (!PlayerState::ReadFrameDisplay(display))
		return;

	if (display.startup == 0 || display.total == 0)
		return;

	for (int p = 0; p < FrameMeter::kPlayers; ++p)
	{
		if (!g_hadAttackBoxes[p] || !g_lastMove[p].valid)
			continue;

		const int startupDelta = g_lastMove[p].startup - display.startup;
		const int totalDelta = g_lastMove[p].total - display.total;
		const bool match = startupDelta == 0 && totalDelta == 0;

		++g_oracleChecks;
		if (!match)
			++g_oracleMismatches;

		LOG("oracle: p%d  startup %d/%d (%+d)  total %d/%d (%+d)  %s   [%d of %d disagree]",
			p, g_lastMove[p].startup, display.startup, startupDelta,
			g_lastMove[p].total, display.total, totalDelta,
			match ? "match" : "MISMATCH",
			g_oracleMismatches, g_oracleChecks);
	}
}

void LogExchange()
{
	if (!g_hasAdvantage)
		return;

	LOG("meter: advantage %+d%s  p0 free@%d stunEnd@%d (tail %d)  p1 free@%d stunEnd@%d (tail %d)",
		g_advantage, g_wasTrade ? " (trade)" : "",
		g_returnedAt[0], g_stunEndedAt[0],
		g_stunEndedAt[0] >= 0 ? g_stunEndedAt[0] - g_returnedAt[0] : 0,
		g_returnedAt[1], g_stunEndedAt[1],
		g_stunEndedAt[1] >= 0 ? g_stunEndedAt[1] - g_returnedAt[1] : 0);

	PlayerState::FrameDisplay display = {};
	if (PlayerState::ReadFrameDisplay(display) && display.startup != 0)
	{
		const int delta = g_advantage - display.advantage;
		const int inBars = g_returnedAt[1] - g_returnedAt[0];

		int endsAt[FrameMeter::kPlayers] = { -1, -1 };

		for (int p = 0; p < FrameMeter::kPlayers; ++p)
		{
			for (int i = g_length - 1; i >= 0; --i)
			{
				const FrameMeter::State state = g_frames[p][i].state;

				if (state != FrameMeter::State::Idle && state != FrameMeter::State::None)
				{
					endsAt[p] = i;
					break;
				}
			}
		}

		const int barsEnd = endsAt[1] - endsAt[0];

		const int heldDelta = g_heldUntilFrame[1] - g_heldUntilFrame[0];

		const bool haveCancel = g_sawCancelFlags[0] && g_sawCancelFlags[1];
		const int cancelDelta = g_cancelFreeFrame[1] - g_cancelFreeFrame[0];

		LOG("oracle: advantage meter %+d  game %+d  delta %+d  %s%s   [bars %+d, barsEnd %+d, "
			"held %+d, frames p0@%d p1@%d]",
			g_advantage, display.advantage, delta, delta == 0 ? "match" : "MISMATCH",
			g_hasAfterRecovery ? "  (bracket shown)" : "", inBars, barsEnd, heldDelta,
			g_returnedFrame[0], g_returnedFrame[1]);

		if (haveCancel)
		{
			LOG("oracle: cancel %+d  (%s the game)   [free p0@%d p1@%d, flags p0=0x%02x p1=0x%02x, "
				"meter free p0@%d p1@%d]",
				cancelDelta,
				cancelDelta == display.advantage ? "matches" : "does not match",
				g_cancelFreeFrame[0], g_cancelFreeFrame[1],
				g_cancelFlagsAtFree[0], g_cancelFlagsAtFree[1],
				g_returnedFrame[0], g_returnedFrame[1]);
		}
	}
}

void FinishExchange()
{
	g_recording = false;
	g_hasAdvantage = false;
	ComputeAdvantage();
	LogOracle();
	LogExchange();
	LogTrace();
}

}

void FrameMeter::Reset()
{
	g_length = 0;
	g_recording = false;
	g_idleRun = 0;
	g_flash.Reset();
	g_hasAdvantage = false;
	g_advantage = 0;
	g_advantageAttacker = -1;
	g_hasBattleFrame = false;

	for (int p = 0; p < kPlayers; ++p)
		g_returnedAt[p] = -1;

	for (int p = 0; p < kPlayers; ++p)
	{
		g_tracked[p] = Tracked();
		g_lastMove[p] = MoveSpan();
		g_hadAttackBoxes[p] = false;
		g_returnedAt[p] = -1;
	}

	g_trackedCount = 0;
	g_framesUntilRefresh = 0;
}

void FrameMeter::SampleFromGameThread()
{
	if (!GameState::AllowsTrainingTools())
	{
		if (g_length != 0 || g_recording)
			Reset();
		return;
	}

	if (!BattleFrameAdvanced())
		return;

	if (--g_framesUntilRefresh <= 0)
	{
		RefreshEntities();
		g_framesUntilRefresh = kEntityRefreshFrames;
	}

	if (g_trackedCount == 0)
		return;

	PlayerState::State states[kPlayers] = {};
	bool valid[kPlayers] = {};
	State classified[kPlayers] = {};
	bool anyBusy = false;

	{
		Profiler::Scope scope(Profiler::Section_TickPlayerState);

		for (int p = 0; p < kPlayers && p < g_trackedCount; ++p)
			valid[p] = PlayerState::Read(g_tracked[p].entity, states[p]);
	}

	int frozen = 0;
	int live = 0;

	for (int p = 0; p < kPlayers && p < g_trackedCount; ++p)
	{
		if (!valid[p])
			continue;

		++live;
		frozen += states[p].hitstop > 0 ? 1 : 0;
	}

	g_flash.Update(states, valid, g_trackedCount);

	const bool flashed = g_flash.IsFreezing();

	for (int p = 0; p < kPlayers; ++p)
	{
		if (p >= g_trackedCount || !valid[p])
			continue;

		classified[p] = Classify(p, states[p]);
		CheckAutoPause(p, classified[p]);
		g_tracked[p].lastState = classified[p];

		if (CommandIsNamed(g_tracked[p].charaNumber, states[p].command))
		{
			g_tracked[p].lastCommand = states[p].command;
		}
		else if (classified[p] == State::Idle)
		{
			g_tracked[p].lastCommand = 0;
		}

		if (classified[p] != State::Idle ||
			(states[p].actionKind & GameOffsets::kActionKindTechAction) != 0)
		{
			anyBusy = true;
		}
	}

	if (!g_recording)
	{
		if (!anyBusy)
			return;

		BeginExchange();
	}

	UpdateCounters(states, valid, classified, g_trackedCount);

	for (int p = 0; p < kPlayers && p < g_trackedCount; ++p)
		CheckCountAutoPause(p);

	const bool inHitstop = live > 0 && frozen == live;
	const bool frozenTick = g_flash.IsRunning() ? flashed : inHitstop;

	const bool record = !frozenTick && (anyBusy || g_idleRun < kIdlePadding);

	if (g_diagEnabled && g_diagLength < kCapacity)
	{
		DiagFrame& d = g_diag[g_diagLength++];
		d.recorded = record;
		d.flashing = flashed;
		d.barIndex = g_length;

		for (int p = 0; p < kPlayers; ++p)
		{
			const bool sampled = p < g_trackedCount && valid[p];
			d.pattern[p] = sampled ? states[p].pattern : 0;
			d.actionable[p] = sampled ? states[p].actionableRaw : 0;
			d.hitstop[p] = sampled ? static_cast<uint16_t>(states[p].hitstop) : 0;
			d.stun[p] = sampled ? static_cast<uint16_t>(states[p].stunTimer) : 0;
			d.attackBoxes[p] = sampled ? static_cast<int16_t>(states[p].attackBoxes) : 0;
			d.shield[p] = sampled ? states[p].shield : 0;
			d.state[p] = static_cast<uint8_t>(sampled ? classified[p] : State::None);

			for (int a = 0; a < 4; ++a)
				d.counts[p][a] = sampled ? static_cast<uint8_t>(states[p].boxCounts[a]) : 0;

			d.boxMask[p] = sampled ? states[p].normalBoxMask : 0;
			d.invulnKind[p] = sampled ? states[p].invulnKind : 0;
			d.counterState[p] = sampled ? states[p].counterState : 0;
			d.techWindow[p] = sampled ? states[p].techWindow : 0;
			d.ukemiTime[p] = sampled ? states[p].ukemiTime : 0;
			d.shieldSuccess[p] = sampled ? states[p].shieldSuccess : 0;
			d.muteki[p][0] = sampled ? states[p].mutekiStrike : 0;
			d.muteki[p][1] = sampled ? states[p].mutekiThrow : 0;
			d.projectile[p] = sampled && states[p].projectileActive;
			d.teching[p] = sampled &&
				(states[p].actionKind & GameOffsets::kActionKindTech) != 0;
			d.actionLock[p] = sampled ? states[p].actionLock : 0;
			d.actionKind[p] = sampled ? states[p].actionKind : 0;
			d.command[p] = sampled ? states[p].command : 0;
			d.inReaction[p] = sampled && states[p].inReaction;
			d.mvCountFrame[p] = sampled ? states[p].mvCountFrame : 0;
			d.airJumpOK[p] = sampled && states[p].airJumpOK != 0;
			d.stance[p] = sampled ? PlayerState::GetStance(states[p]) : GameOffsets::kStatusStand;
			d.positionY[p] = sampled ? states[p].positionY : 0;
			d.moveCodeEx1[p] = sampled ? states[p].moveCodeEx[1] : 0;
			d.moveCodeEx2[p] = sampled ? states[p].moveCodeEx[2] : 0;
			d.moveCodeEx3[p] = sampled ? states[p].moveCodeEx[3] : 0;
			d.moveCodeEx7[p] = sampled ? states[p].moveCodeEx[7] : 0;
			d.attrInvuln[p] = sampled ? states[p].attrInvuln : 0;
			d.invuln[p] = sampled ? DrawnInvuln(states[p], classified[p]) : 0;
			d.atemiBox[p] = sampled ? states[p].atemiBox : -1;
			d.cancelFree[p] = sampled && states[p].cancelFree;
			d.cancelNormal[p] = sampled ? states[p].cancelNormal : 0;
			d.cancelSpecial[p] = sampled ? states[p].cancelSpecial : 0;
			d.hurtboxes[p] = sampled ? states[p].hurtboxCount : 0;

			d.freeFrom[p] = g_freeFrom[p];
			d.moveEndAt[p] = g_moveEndAt[p];
			d.freeAfterTech[p] = g_techStartedAt[p];
		}
	}

	bool teching[kPlayers] = {};
	for (int p = 0; p < kPlayers && p < g_trackedCount; ++p)
	{
		teching[p] = valid[p] &&
			(states[p].actionKind & GameOffsets::kActionKindTech) != 0;
	}

	for (int p = 0; p < kPlayers; ++p)
	{
		if (record)
		{
			Frame& frame = g_frames[p][g_length];

			if (p >= g_trackedCount || !valid[p])
			{
				frame.state = State::None;
				frame.projectileActive = false;
				frame.teching = false;
				frame.invuln = 0;
				frame.airJumpOK = false;
				frame.pattern = 0;
			}
			else
			{
				frame.state = classified[p];
				frame.projectileActive = states[p].projectileActive;
				frame.teching = teching[p];
				frame.airJumpOK = states[p].airJumpOK != 0;
				frame.pattern = states[p].pattern;
				frame.invuln = DrawnInvuln(states[p], classified[p]);
			}
		}

		if (p >= g_trackedCount || !valid[p])
			continue;

		if (states[p].attackBoxes > 0 || states[p].projectileActive)
			g_hadAttackBoxes[p] = true;

		if (!states[p].actionable && states[p].mutekiThrow > 0 && states[p].mutekiStrike == 0 &&
			states[p].actionKind == 0)
		{
			g_downEndAt[p] = g_length;
			g_downEndFrame[p] = g_frame;
		}
		else if (states[p].actionKind != 0)
		{

			g_downEndAt[p] = -1;
		}

		if (!g_tracked[p].canAct)
		{
			g_heldUntilFrame[p] = g_frame;
			g_heldUntilBar[p] = g_length;
		}

		if (states[p].hasCancelFlags)
		{
			g_sawCancelFlags[p] = true;

			if (states[p].cancelFlagsA == 0 && states[p].cancelFlagsB == 0)
			{
				g_cancelFreeFrame[p] = g_frame;
			}
			else if (g_cancelFlagsAtFree[p] == 0)
			{
				g_cancelFlagsAtFree[p] =
					static_cast<uint8_t>(states[p].cancelFlagsA | states[p].cancelFlagsB);
			}
		}

		if (g_tracked[p].canAct)
		{
			if (g_freeFrom[p] < 0)
			{
				const int lockFree = g_tracked[p].lockFreeAt;

				const bool backDate = lockFree >= 0 && lockFree < g_length;

				g_freeFrom[p] = backDate ? lockFree : g_length;
				g_freeFromFrame[p] = backDate ? g_tracked[p].lockFreeFrame : g_frame;
				g_freePattern[p] = states[p].pattern;
				g_moveEndAt[p] = -1;
				g_moveEndFrame[p] = -1;

				g_lockHeldAtFree[p] = states[p].actionLock != 0 &&
					(states[p].actionKind & GameOffsets::kActionKindOwnMove) != 0;
				g_lockEndAt[p] = -1;
				g_lockEndFrame[p] = -1;

				g_downTakenAt[p] = g_downEndAt[p] >= 0 && g_downEndAt[p] < g_freeFrom[p]
					? g_downEndAt[p] : -1;
				g_downTakenFrame[p] = g_downTakenAt[p] >= 0 ? g_downEndFrame[p] : -1;
			}
		}
		else if (g_freeFrom[p] >= 0)
		{
			if (states[p].stunTimer > 0 || teching[p] ||
				states[p].pattern != g_freePattern[p])
			{

				g_freeFrom[p] = -1;
				g_freeFromFrame[p] = -1;
				g_moveEndAt[p] = -1;
				g_moveEndFrame[p] = -1;

				g_lockEndAt[p] = -1;
				g_lockEndFrame[p] = -1;
				g_downEndAt[p] = -1;
				g_downEndFrame[p] = -1;
				g_downTakenAt[p] = -1;
				g_downTakenFrame[p] = -1;
			}
			else if (g_moveEndAt[p] < 0)
			{

				g_moveEndAt[p] = g_length;
				g_moveEndFrame[p] = g_frame;
			}
		}

		if (g_lockHeldAtFree[p] && g_lockEndAt[p] < 0 && g_freeFrom[p] >= 0 &&
			g_length > g_freeFrom[p] && states[p].actionLock == 0)
		{
			g_lockEndAt[p] = g_length;
			g_lockEndFrame[p] = g_frame;
		}

		g_returnedAt[p] = g_lockEndAt[p] >= 0 ? g_lockEndAt[p]
			: (g_moveEndAt[p] >= 0 ? g_moveEndAt[p]
			: (g_downTakenAt[p] >= 0 ? g_downTakenAt[p] : g_freeFrom[p]));

		g_returnedFrame[p] = g_lockEndFrame[p] >= 0 ? g_lockEndFrame[p]
			: (g_moveEndFrame[p] >= 0 ? g_moveEndFrame[p]
			: (g_downTakenFrame[p] >= 0 ? g_downTakenFrame[p] : g_freeFromFrame[p]));

		if ((states[p].actionKind & GameOffsets::kActionKindTechAction) != 0)
		{
			if (!g_techSeen[p])
			{
				g_techSeen[p] = true;
				g_techStartedAt[p] = g_length;
				g_techStartedFrame[p] = g_frame;
			}

			g_techEndedAt[p] = g_length + 1;
			g_techEndedFrame[p] = g_frame + 1;
		}

		if (InStunPattern(states[p].pattern))
			g_stunEndedAt[p] = g_length + 1;
	}

	if (record)
	{
		++g_length;

		for (int p = 0; p < kPlayers && p < g_trackedCount; ++p)
			UpdateLastMoveFromBar(p);
	}

	++g_frame;

	if (frozenTick)
		return;

	g_idleRun = anyBusy ? 0 : g_idleRun + 1;

	if (!anyBusy && g_idleRun >= kIdleFramesToEnd)
	{
		FinishExchange();
		return;
	}

	if (g_length >= kCapacity)
		DropOldestFrames(kCapacity / 4);
}

FrameMeter::AutoPauseConfig FrameMeter::GetAutoPause()
{
	return g_autoPause;
}

void FrameMeter::SetAutoPause(const AutoPauseConfig& config)
{
	g_autoPause = config;
}

int FrameMeter::GetComboCount(int player)
{
	if (player < 0 || player >= kPlayers)
		return 0;

	return g_tracked[player].comboCount;
}

int FrameMeter::GetBlockedRun(int player)
{
	if (player < 0 || player >= kPlayers)
		return 0;

	return g_tracked[player].blockedRun;
}

bool FrameMeter::IsSuperFlashRunning()
{
	return g_flash.IsRunning();
}

int FrameMeter::GetFlashFrames(int player)
{
	return g_flash.GetFrames(player);
}

bool FrameMeter::ConsumeMatchRestart()
{
	if (!g_restartPending)
		return false;

	g_restartPending = false;
	return true;
}

bool FrameMeter::IsPlayerFree(int player)
{
	if (player < 0 || player >= kPlayers)
		return false;

	return g_tracked[player].canAct;
}

int FrameMeter::GetBlockedTotal(int player)
{
	if (player < 0 || player >= kPlayers)
		return 0;

	return g_tracked[player].blockedTotal;
}

int FrameMeter::PackAutoPause(const AutoPauseConfig& config)
{
	int packed = 0;

	if (config.player[0])
		packed |= 1;
	if (config.player[1])
		packed |= 2;
	if (config.onMoveStarts)
		packed |= 4;
	if (config.onHit)
		packed |= 8;
	if (config.onComboHits)
		packed |= 16;
	if (config.onBlockedHits)
		packed |= 32;
	if (config.blockedAllowsGaps)
		packed |= 64;
	if (config.resumeDelay)
		packed |= 128;
	if (config.delayOnManualPause)
		packed |= 256;
	if (config.delayOnMoveStarts)
		packed |= 512;
	if (config.delayOnHitLands)
		packed |= 1024;
	if (config.delayOnComboReaches)
		packed |= 2048;
	if (config.delayOnBlockReaches)
		packed |= 4096;
	if (config.onDummyRecord)
		packed |= 8192;
	if (config.onDummyPlayback)
		packed |= 16384;
	if (config.leadInMode == LeadIn_Still)
		packed |= 32768;
	if (config.reversalAfterRestart)
		packed |= 65536;
	if (config.reversalAfterCountdown)
		packed |= 131072;
	if (config.showP2Inputs)
		packed |= 262144;

	return packed;
}

FrameMeter::AutoPauseConfig FrameMeter::UnpackAutoPause(int packed)
{
	AutoPauseConfig config = {};
	config.player[0] = (packed & 1) != 0;
	config.player[1] = (packed & 2) != 0;
	config.onMoveStarts = (packed & 4) != 0;
	config.onHit = (packed & 8) != 0;
	config.onComboHits = (packed & 16) != 0;
	config.onBlockedHits = (packed & 32) != 0;
	config.blockedAllowsGaps = (packed & 64) != 0;
	config.resumeDelay = (packed & 128) != 0;
	config.delayOnManualPause = (packed & 256) != 0;
	config.delayOnMoveStarts = (packed & 512) != 0;
	config.delayOnHitLands = (packed & 1024) != 0;
	config.delayOnComboReaches = (packed & 2048) != 0;
	config.delayOnBlockReaches = (packed & 4096) != 0;
	config.onDummyRecord = (packed & 8192) != 0;
	config.onDummyPlayback = (packed & 16384) != 0;
	config.leadInMode = (packed & 32768) != 0 ? LeadIn_Still : LeadIn_WarmUp;
	config.reversalAfterRestart = (packed & 65536) != 0;
	config.reversalAfterCountdown = (packed & 131072) != 0;
	config.showP2Inputs = (packed & 262144) != 0;

	config.comboStop[0] = 3;
	config.blockStop[0] = 3;
	config.resumeDelayFrames = 60;

	return config;
}

bool FrameMeter::IsRecording()
{
	return g_recording;
}

int FrameMeter::GetLength()
{
	return g_length;
}

bool FrameMeter::GetFrame(int player, int index, Frame& out)
{
	if (player < 0 || player >= kPlayers || index < 0 || index >= g_length)
		return false;

	out = g_frames[player][index];
	return true;
}

bool FrameMeter::HasAdvantage()
{
	return g_hasAdvantage;
}

int FrameMeter::GetAdvantage()
{
	return g_advantage;
}

bool FrameMeter::GetAdvantageFor(int player, int& outAdvantage)
{
	if (!g_hasAdvantage || player < 0 || player >= kPlayers)
		return false;

	outAdvantage = player == 0 ? g_advantage : -g_advantage;
	return true;
}

bool FrameMeter::GetAdvantageAfterRecoveryFor(int player, int& outAdvantage)
{
	if (!g_hasAdvantage || !g_hasAfterRecovery || player < 0 || player >= kPlayers)
		return false;

	outAdvantage = player == 0 ? g_advantageAfterRecovery : -g_advantageAfterRecovery;
	return true;
}

bool FrameMeter::WasTrade()
{
	return g_hasAdvantage && g_wasTrade;
}

void FrameMeter::SetTraceEnabled(bool enabled)
{
	g_diagEnabled = enabled;
}

bool FrameMeter::IsTraceEnabled()
{
	return g_diagEnabled;
}

void FrameMeter::ShutdownTrace()
{
	if (g_traceThread == nullptr)
		return;

	InterlockedExchange(&g_traceStop, 1);
	SetEvent(g_traceSignal);
	WaitForSingleObject(g_traceThread, 300);

	CloseHandle(g_traceThread);
	CloseHandle(g_traceSignal);

	g_traceThread = nullptr;
	g_traceSignal = nullptr;
}

int FrameMeter::GetTrailingRunLength(int player)
{
	if (player < 0 || player >= kPlayers || g_length == 0)
		return 0;

	int run = 0;
	for (int i = g_length - 1; i >= 0; --i)
	{
		const Frame& frame = g_frames[player][i];
		if (frame.state == State::Idle || frame.state == State::None)
			break;

		++run;
	}

	return run;
}

const char* FrameMeter::GetActionName(int player)
{
	if (player < 0 || player >= kPlayers)
		return "";

	const char* const move =
		CharaTables::MoveName(g_tracked[player].charaNumber, g_tracked[player].lastCommand);

	if (move[0] != '\0')
		return move;

	return GameTables::CommandName(g_tracked[player].lastCommand);
}

bool FrameMeter::GetCharaAndPattern(int player, int& outChara, int& outPattern)
{
	if (player < 0 || player >= kPlayers || player >= g_trackedCount)
		return false;

	if (g_tracked[player].entity == nullptr || g_tracked[player].charaNumber < 0)
		return false;

	outChara = g_tracked[player].charaNumber;
	outPattern = g_tracked[player].pattern;
	return true;
}

bool FrameMeter::GetLastMove(int player, int& outStartup, int& outActive, int& outRecovery,
	int& outTotal)
{
	if (player < 0 || player >= kPlayers || !g_lastMove[player].valid)
		return false;

	outStartup = g_lastMove[player].startup;
	outActive = g_lastMove[player].active;
	outRecovery = g_lastMove[player].recovery;
	outTotal = g_lastMove[player].total;
	return true;
}

const char* FrameMeter::GetStateName(State state)
{
	switch (state)
	{
	case State::Idle:      return "Free";
	case State::Startup:   return "Startup";
	case State::Armor:     return "Armor";
	case State::Active:    return "Active";
	case State::Recovery:  return "Recovery";
	case State::Cancellable: return "Cancellable";
	case State::Blockstun: return "Blockstun";
	case State::Hitstun:   return "Hitstun";
	case State::Parry:     return "Parry";
	case State::Movement:  return "Dash";
	case State::Jump:      return "Jump";
	case State::AirMovement: return "Air Movement";
	case State::Dodge:     return "Dodge";
	default:               return "";
	}
}

static constexpr uint32_t Argb(uint32_t r, uint32_t g, uint32_t b)
{
	return (255u << 24) | (r << 16) | (g << 8) | b;
}

uint32_t FrameMeter::GetStateColor(State state)
{
	switch (state)
	{
	case State::Idle:      return Argb(64, 72, 86);
	case State::Startup:   return Argb(64, 182, 226);
	case State::Armor:     return Argb(158, 92, 214);
	case State::Active:    return Argb(214, 48, 52);
	case State::Recovery:  return Argb(46, 84, 176);

	case State::Cancellable: return Argb(64, 72, 86);
	case State::Blockstun: return Argb(86, 176, 126);
	case State::Hitstun:   return Argb(226, 132, 48);

	case State::Parry:     return Argb(96, 58, 146);

	case State::Movement:  return Argb(212, 184, 72);

	case State::Jump:        return Argb(36, 132, 180);
	case State::AirMovement: return Argb(112, 124, 236);

	case State::Dodge:       return Argb(186, 74, 168);
	default:               return Argb(20, 22, 28);
	}
}

const char* FrameMeter::GetMarkerName(Marker marker)
{
	switch (marker)
	{
	case Marker_Projectile:       return "Projectile Active";
	case Marker_Tech:             return "Teching";
	case Marker_FullInvuln:       return "Full Invincibility";
	case Marker_StrikeInvuln:     return "Strike Invincibility";
	case Marker_ThrowInvuln:      return "Throw Invincibility";
	case Marker_ProjectileInvuln: return "Projectile Invincibility";
	case Marker_HeadInvuln:       return "Head Invincibility";
	case Marker_BodyInvuln:       return "Body Invincibility";
	case Marker_LegsInvuln:       return "Legs Invincibility";
	case Marker_DiveInvuln:       return "Air Dive Invincibility";
	case Marker_HighMidInvuln:    return "High and Mid Invincibility";
	case Marker_LowMidInvuln:     return "Low and Mid Invincibility";
	default:                      return "";
	}
}

uint16_t FrameMeter::GetMarkerInvulnBit(Marker marker)
{
	switch (marker)
	{
	case Marker_FullInvuln:       return PlayerState::Invuln_Full;
	case Marker_StrikeInvuln:     return PlayerState::Invuln_Strike;
	case Marker_ThrowInvuln:      return PlayerState::Invuln_Throw;
	case Marker_ProjectileInvuln: return PlayerState::Invuln_Projectile;
	case Marker_HeadInvuln:       return PlayerState::Invuln_Head;
	case Marker_BodyInvuln:       return PlayerState::Invuln_Body;
	case Marker_LegsInvuln:       return PlayerState::Invuln_Legs;
	case Marker_DiveInvuln:       return PlayerState::Invuln_Dive;
	case Marker_HighMidInvuln:    return PlayerState::Invuln_HighMid;
	case Marker_LowMidInvuln:     return PlayerState::Invuln_LowMid;
	default:                      return 0;
	}
}

uint32_t FrameMeter::GetMarkerColor(Marker marker)
{
	switch (marker)
	{
	case Marker_Projectile:       return Argb(240, 170, 40);
	case Marker_Tech:             return Argb(120, 240, 190);

	case Marker_FullInvuln:       return Argb(255, 255, 255);

	case Marker_StrikeInvuln:     return Argb(60, 140, 255);
	case Marker_ThrowInvuln:      return Argb(255, 210, 74);

	case Marker_ProjectileInvuln: return Argb(0, 200, 170);
	case Marker_HeadInvuln:       return Argb(255, 122, 47);
	case Marker_BodyInvuln:       return Argb(124, 224, 74);
	case Marker_LegsInvuln:       return Argb(180, 90, 255);
	case Marker_DiveInvuln:       return Argb(255, 79, 208);
	case Marker_HighMidInvuln:    return Argb(90, 200, 220);
	case Marker_LowMidInvuln:     return Argb(220, 60, 60);
	default:                      return Argb(255, 255, 255);
	}
}
