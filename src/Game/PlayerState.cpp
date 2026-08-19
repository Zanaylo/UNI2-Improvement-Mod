#include "Game/PlayerState.h"

#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/HitboxData.h"
#include "Game/MemoryMap.h"

#include <cstring>

namespace {

thread_local uint8_t t_snapshot[GameOffsets::kPlayerDataSize] = {};
thread_local const void* t_snapshotOwner = nullptr;

struct Snapshot
{
	explicit Snapshot(void* playerData)
	{
		t_snapshotOwner = TryReadMemory(t_snapshot, playerData, GameOffsets::kPlayerDataSize)
			? playerData : nullptr;
	}

	~Snapshot()
	{
		t_snapshotOwner = nullptr;
	}

	Snapshot(const Snapshot&) = delete;
	Snapshot& operator=(const Snapshot&) = delete;
};

template <typename T>
bool ReadAt(void* object, uintptr_t offset, T& out)
{
	if (object == t_snapshotOwner && offset + sizeof(T) <= GameOffsets::kPlayerDataSize)
	{
		memcpy(&out, t_snapshot + offset, sizeof(T));
		return true;
	}

	return TryReadMemory(&out, reinterpret_cast<const uint8_t*>(object) + offset, sizeof(T));
}

uint32_t ReadTimedRecord(void* playerData, uintptr_t valueOffset, uintptr_t timeOffset)
{
	int32_t time = 0;
	if (!ReadAt(playerData, timeOffset, time) || time <= 0)
		return 0;

	uint32_t value = 0;
	if (!ReadAt(playerData, valueOffset, value))
		return 0;

	return value;
}

bool ResolveFrameRecord(void* playerData, uint32_t& outRecord)
{
	outRecord = 0;

	uint32_t frameObject = 0;
	if (!ReadAt(playerData, GameOffsets::kCharaFrameObject, frameObject) || frameObject == 0)
		return false;

	uint32_t record = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(frameObject + GameOffsets::kFrameObjectRecord),
		record) || record == 0)
	{
		return false;
	}

	outRecord = record;
	return true;
}

void ReadCancelFlags(void* playerData, uint32_t record, PlayerState::State& out)
{
	out.hasCancelFlags = false;
	out.cancelFlagsA = 0;
	out.cancelFlagsB = 0;

	if (record == 0)
		return;

	uint8_t a = 0;
	uint8_t b = 0;
	if (!TryReadMemory(&a, reinterpret_cast<const void*>(record + GameOffsets::kCancelFlagsA),
			sizeof(a)) ||
		!TryReadMemory(&b, reinterpret_cast<const void*>(record + GameOffsets::kCancelFlagsB),
			sizeof(b)))
	{
		return;
	}

	out.hasCancelFlags = true;
	out.cancelFlagsA = a;
	out.cancelFlagsB = b;
	out.cancelNormal = a;
	out.cancelSpecial = b;

	int32_t boostTime = 0;
	if (!ReadAt(playerData, GameOffsets::kPlayerDataCancelBoostTime, boostTime) || boostTime <= 0)
		return;

	uint16_t boost = 0;
	if (!ReadAt(playerData, GameOffsets::kPlayerDataCancelBoost, boost))
		return;

	const uint8_t normal = static_cast<uint8_t>(boost & 0xff);
	const uint8_t special = static_cast<uint8_t>(boost >> 8);

	if (normal != GameOffsets::kCancelBoostNone)
		out.cancelNormal = normal;
	if (special != GameOffsets::kCancelBoostNone)
		out.cancelSpecial = special;
}

void ReadCancelFree(void* playerData, uint32_t frameRecord, PlayerState::State& out)
{
	out.cancelFree = true;

	for (int i = 0; i < GameOffsets::kPlayerDataCancelOverrideCount; ++i)
	{
		const uintptr_t overrideRecord = GameOffsets::kPlayerDataCancelOverride +
			static_cast<uintptr_t>(i) * GameOffsets::kPlayerDataCancelOverrideStride;

		int32_t time = 0;
		if (!ReadAt(playerData, overrideRecord + 0xc, time) || time <= 0)
			continue;

		uint8_t value = 0;
		out.cancelFree = ReadAt(playerData, overrideRecord, value) ? value != 0 : true;
		return;
	}

	if (frameRecord == 0)
		return;

	uint8_t value = 0;
	if (TryReadMemory(&value, reinterpret_cast<const void*>(
		frameRecord + GameOffsets::kFrameObjectCancelFree), sizeof(value)))
	{
		out.cancelFree = value != 0;
	}
}

void ReadBoxPicture(void* playerData, PlayerState::State& out)
{
	out.attackBoxes = -1;
	out.normalBoxMask = 0;
	out.counterWindow = false;
	for (int i = 0; i < 4; ++i)
		out.boxCounts[i] = 0;

	HitboxData::FrameObject frame = {};
	if (!HitboxData::Resolve(playerData, frame))
		return;

	for (int i = 0; i < HitboxData::kArrayCount && i < 4; ++i)
		out.boxCounts[i] = frame.counts[i];

	HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
	const int count = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);

	out.attackBoxes = 0;

	for (int i = 0; i < count; ++i)
	{
		if (boxes[i].arrayIndex == HitboxData::BoxArray_Attack)
		{
			++out.attackBoxes;
			continue;
		}

		if (boxes[i].arrayIndex != HitboxData::BoxArray_Normal)
			continue;

		if (boxes[i].index >= 0 && boxes[i].index < 32)
			out.normalBoxMask |= 1u << boxes[i].index;
	}
}

void ReadFrameRecord(uint32_t record, PlayerState::State& out)
{
	out.stance = GameOffsets::kStatusStand;
	out.invulnKind = 0;

	if (record == 0)
		return;

	TryReadMemory(&out.stance,
		reinterpret_cast<const void*>(record + GameOffsets::kFrameRecordStance), sizeof(out.stance));
	TryReadMemory(&out.invulnKind,
		reinterpret_cast<const void*>(record + GameOffsets::kFrameRecordInvulnKind),
		sizeof(out.invulnKind));
}

bool HasActiveProjectile(void* playerData, uint32_t ownedObjects)
{
	if (ownedObjects == 0)
		return false;

	void* effects[MemoryMap::kEffectCacheMax] = {};
	const int count = MemoryMap::EnumerateEffectSlotsCached(effects, MemoryMap::kEffectCacheMax);

	for (int i = 0; i < count; ++i)
	{
		uint32_t owner = 0;
		if (!MemoryMap::ReadStructDword(effects[i], GameOffsets::kCharaOwner, owner))
			continue;

		if (reinterpret_cast<void*>(owner) != playerData)
			continue;

		HitboxData::FrameObject frame = {};
		if (!HitboxData::Resolve(effects[i], frame))
			continue;

		HitboxData::Box boxes[HitboxData::kMaxBoxes] = {};
		const int count = HitboxData::ReadBoxes(frame, boxes, HitboxData::kMaxBoxes);

		for (int b = 0; b < count; ++b)
		{
			if (boxes[b].arrayIndex == HitboxData::BoxArray_Attack)
				return true;
		}
	}

	return false;
}

}

bool PlayerState::Read(void* playerData, State& out)
{
	if (playerData == nullptr)
		return false;

	out = State();

	const Snapshot snapshot(playerData);

	uint32_t frameUpdate = 0;
	const bool core =
		ReadAt(playerData, GameOffsets::kPlayerDataPattern, out.pattern) &&
		ReadAt(playerData, GameOffsets::kPlayerDataFrameIndex, out.frameIndex) &&
		ReadAt(playerData, GameOffsets::kPlayerDataFrameFlag, out.frameFlag) &&
		ReadAt(playerData, GameOffsets::kPlayerDataFrameUpdate, frameUpdate) &&
		ReadAt(playerData, GameOffsets::kPlayerDataMvCountFrame, out.mvCountFrame) &&
		ReadAt(playerData, GameOffsets::kPlayerDataMvFlagA, out.mvFlagA) &&
		ReadAt(playerData, GameOffsets::kPlayerDataMvFlagB, out.mvFlagB) &&
		ReadAt(playerData, GameOffsets::kPlayerDataMvValue, out.mvValue);

	if (!core)
		return false;

	out.frameUpdate = frameUpdate == 1;

	uint32_t actionable = 0;
	ReadAt(playerData, GameOffsets::kPlayerDataActionable, actionable);
	out.actionable = actionable != 0;
	out.actionableRaw = actionable;

	if (!ReadAt(playerData, GameOffsets::kPlayerDataActionLock, out.actionLock))
		out.actionLock = 0;

	for (int i = 0; i < GameOffsets::kMoveCodeExCount; ++i)
	{
		if (!ReadAt(playerData,
			GameOffsets::kPlayerDataMoveCodeEx + static_cast<uintptr_t>(i) * 4, out.moveCodeEx[i]))
		{
			out.moveCodeEx[i] = 0;
		}
	}

	out.actionKind = out.moveCodeEx[0];

	uint32_t frameRecord = 0;
	ResolveFrameRecord(playerData, frameRecord);

	ReadCancelFlags(playerData, frameRecord, out);
	ReadCancelFree(playerData, frameRecord, out);

	if (!ReadAt(playerData, GameOffsets::kPlayerDataCommand, out.command))
		out.command = 0;

	ReadAt(playerData, GameOffsets::kPlayerDataRunning, out.running);
	ReadAt(playerData, GameOffsets::kPlayerDataAirJumpOK, out.airJumpOK);

	ReadAt(playerData, GameOffsets::kPlayerDataPosStateValue, out.stanceOverride);
	if (!ReadAt(playerData, GameOffsets::kPlayerDataPosStateTime, out.stanceOverrideTime))
		out.stanceOverrideTime = 0;

	int32_t baseY = 0;
	int32_t offsetY = 0;
	if (ReadAt(playerData, GameOffsets::kPlayerDataBaseY, baseY) &&
		ReadAt(playerData, GameOffsets::kPlayerDataOffsetY, offsetY))
	{
		out.positionY = baseY + offsetY;
	}

	ReadAt(playerData, GameOffsets::kPlayerDataHitstop, out.hitstop);
	ReadAt(playerData, GameOffsets::kPlayerDataStunTimer, out.stunTimer);

	uint8_t a0 = 0, b0 = 0, a1 = 0, b1 = 0;
	ReadAt(playerData, GameOffsets::kPlayerDataMutekiA0, a0);
	ReadAt(playerData, GameOffsets::kPlayerDataMutekiB0, b0);
	ReadAt(playerData, GameOffsets::kPlayerDataMutekiA1, a1);
	ReadAt(playerData, GameOffsets::kPlayerDataMutekiB1, b1);
	out.mutekiStrike = a0 > a1 ? a0 : a1;
	out.mutekiThrow = b0 > b1 ? b0 : b1;

	out.attrInvuln = ReadTimedRecord(playerData, GameOffsets::kPlayerDataHitCheckInvuln,
		GameOffsets::kPlayerDataHitCheckInvulnTime);
	out.attackAttrs = ReadTimedRecord(playerData, GameOffsets::kPlayerDataHitCheckAttack,
		GameOffsets::kPlayerDataHitCheckAttackTime);

	int32_t atemiTime = 0;
	ReadAt(playerData, GameOffsets::kPlayerDataAtemiTime, atemiTime);
	out.atemiWindow = atemiTime > 0;
	if (!out.atemiWindow || !ReadAt(playerData, GameOffsets::kPlayerDataAtemiBox, out.atemiBox))
		out.atemiBox = -1;

	uint32_t existFlags = 0;
	ReadAt(playerData, GameOffsets::kCharaExistFlags, existFlags);
	out.existNoKurai = (existFlags & GameOffsets::kExistNoKuraiHantei) != 0;

	if (!ReadAt(playerData, GameOffsets::kPlayerDataHurtboxCount, out.hurtboxCount))
		out.hurtboxCount = 1;

	int32_t armorCount = 0;
	uint8_t armorFlag = 0;
	ReadAt(playerData, GameOffsets::kPlayerDataArmorCount, armorCount);
	ReadAt(playerData, GameOffsets::kPlayerDataArmorFlag, armorFlag);
	out.armor = armorCount > 0 && armorFlag != 0;

	ReadAt(playerData, GameOffsets::kPlayerDataShield, out.shield);
	ReadAt(playerData, GameOffsets::kPlayerDataVGuardTime, out.vguardTime);

	uint32_t context = 0;
	if (ReadAt(playerData, GameOffsets::kCharaFrameObject, context) && context != 0)
	{
		void* ctx = reinterpret_cast<void*>(static_cast<uintptr_t>(context));

		out.hasContext =
			ReadAt(ctx, GameOffsets::kContextStatusWord, out.statusWord) &&
			ReadAt(ctx, GameOffsets::kContextStatusA, out.statusA) &&
			ReadAt(ctx, GameOffsets::kContextStatusB, out.statusB) &&
			ReadAt(ctx, GameOffsets::kContextStatusC, out.statusC) &&
			ReadAt(ctx, GameOffsets::kContextMvStatus, out.mvStatus);
	}

	uint32_t reaction = 0;
	out.inReaction = ReadAt(playerData, GameOffsets::kPlayerDataInReaction, reaction) &&
		reaction != 0;

	if (!ReadAt(playerData, GameOffsets::kPlayerDataTechWindow, out.techWindow))
		out.techWindow = 0;

	if (!ReadAt(playerData, GameOffsets::kPlayerDataUkemiTime, out.ukemiTime))
		out.ukemiTime = 0;

	ReadBoxPicture(playerData, out);

	out.counterWindow = out.atemiWindow &&
		out.atemiBox >= 0 && out.atemiBox < GameOffsets::kEtcBoxCount &&
		(out.normalBoxMask & (1u << (GameOffsets::kEtcBoxFirst + out.atemiBox))) != 0;

	ReadFrameRecord(frameRecord, out);

	if (!ReadAt(playerData, GameOffsets::kCharaCounterState, out.counterState))
		out.counterState = 0;

	if (!ReadAt(playerData, GameOffsets::kPlayerDataShieldSuccess, out.shieldSuccess))
		out.shieldSuccess = 0;

	if (!ReadAt(playerData, GameOffsets::kCharaObjectCount, out.ownedObjects))
		out.ownedObjects = 0;

	out.projectileActive = HasActiveProjectile(playerData, out.ownedObjects);
	return true;
}

uint16_t PlayerState::GetInvuln(const State& state)
{
	const bool discounted = (state.moveCodeEx[3] & GameOffsets::kMoveCode3Anten) != 0 ||
		(state.moveCodeEx[6] & GameOffsets::kMoveCode6SousaiMuteki) != 0;

	uint16_t invuln = 0;

	switch (state.invulnKind)
	{
	case kInvulnHighMid: invuln |= Invuln_HighMid; break;
	case kInvulnLowMid:  invuln |= Invuln_LowMid; break;
	case kInvulnStrike:  invuln |= Invuln_Strike; break;
	case kInvulnThrow:   invuln |= Invuln_Throw; break;
	case kInvulnBoth:    invuln |= Invuln_Full; break;
	default: break;
	}

	if (state.attrInvuln & GameOffsets::kHitCheckHead)     invuln |= Invuln_Head;
	if (state.attrInvuln & GameOffsets::kHitCheckBody)     invuln |= Invuln_Body;
	if (state.attrInvuln & GameOffsets::kHitCheckLegs)     invuln |= Invuln_Legs;
	if (state.attrInvuln & GameOffsets::kHitCheckFireBall) invuln |= Invuln_Projectile;
	if (state.attrInvuln & GameOffsets::kHitCheckThrow)    invuln |= Invuln_Throw;
	if (state.attrInvuln & GameOffsets::kHitCheckAirDive)  invuln |= Invuln_Dive;

	if (!discounted)
	{
		if (state.mutekiStrike > 0)
			invuln |= Invuln_Strike;
		if (state.mutekiThrow > 0)
			invuln |= Invuln_Throw;

		if (state.existNoKurai || state.hurtboxCount == 0)
			invuln |= Invuln_Strike;
	}

	if ((invuln & Invuln_Strike) != 0 && (invuln & Invuln_Throw) != 0)
		invuln |= Invuln_Full;

	if ((invuln & Invuln_Full) != 0)
		return Invuln_Full;

	return invuln;
}

int PlayerState::ReadCatchBoxIndex(void* playerData)
{
	if (playerData == nullptr)
		return -1;

	int32_t time = 0;
	if (!ReadAt(playerData, GameOffsets::kPlayerDataAtemiTime, time) || time <= 0)
		return -1;

	int16_t box = 0;
	if (!ReadAt(playerData, GameOffsets::kPlayerDataAtemiBox, box))
		return -1;

	if (box < 0 || box >= GameOffsets::kEtcBoxCount)
		return -1;

	return GameOffsets::kEtcBoxFirst + box;
}

int PlayerState::ReadFrameDisplayRaw(uint32_t* out, int count)
{
	if (out == nullptr || count <= 0)
		return 0;

	uint32_t object = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kFrameDisplayPointer)),
		object) || object == 0)
	{
		return 0;
	}

	int read = 0;
	for (int i = 0; i < count; ++i)
	{
		if (!TryReadDword(reinterpret_cast<const void*>(object + 0x10 + i * 4), out[i]))
			break;

		++read;
	}

	return read;
}

bool PlayerState::ReadFrameDisplay(FrameDisplay& out)
{
	uint32_t object = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kFrameDisplayPointer)),
		object) || object == 0)
	{
		return false;
	}

	void* base = reinterpret_cast<void*>(static_cast<uintptr_t>(object));

	int32_t startup = 0;
	int32_t total = 0;
	int32_t advantage = 0;

	if (!ReadAt(base, GameOffsets::kFrameDisplayStartup, startup) ||
		!ReadAt(base, GameOffsets::kFrameDisplayTotal, total) ||
		!ReadAt(base, GameOffsets::kFrameDisplayAdvantage, advantage))
	{
		return false;
	}

	if (startup < 0 || startup > 9999 || total < 0 || total > 99999 ||
		advantage < -9999 || advantage > 9999)
	{
		return false;
	}

	out.startup = startup;
	out.total = total;
	out.advantage = advantage;
	return true;
}
