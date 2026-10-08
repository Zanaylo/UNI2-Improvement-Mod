#include "Game/Battle/MatchBlock.h"

#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"

namespace {

typedef void(__fastcall* ApplyBlockFn)(int32_t*);

constexpr int kSides = 2;
constexpr int kRuleSlot = 8;

constexpr uintptr_t kRules[] = {
	GameOffsets::kBattleRuleA, GameOffsets::kBattleRuleB, GameOffsets::kBattleRuleC, GameOffsets::kBattleRuleD,
};

int32_t ReadInt(uintptr_t rva)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value);

	return static_cast<int32_t>(value);
}

}

MatchBlock::Block MatchBlock::Capture()
{
	Block block = {};

	for (int side = 0; side < kSides; ++side)
	{
		const int base = side * kSlotsPerSide;

		block.values[base] = ReadInt(GameOffsets::kBattleCharaRecord + side * GameOffsets::kBattleCharaRecordStride);
		block.values[base + 1] = ReadInt(GameOffsets::kBattleCharaColor + side * GameOffsets::kBattleCharaSlotStride);
		block.values[base + 2] = ReadInt(GameOffsets::kBattleCharaExtra + side * GameOffsets::kBattleCharaSlotStride);
	}

	block.values[kStageSlot] = ReadInt(GameOffsets::kBgPendingNumber);
	block.values[kBgmSlot] = ReadInt(GameOffsets::kBattleBgm);

	for (int i = 0; i < static_cast<int>(sizeof(kRules) / sizeof(kRules[0])); ++i)
		block.values[kRuleSlot + i] = ReadInt(kRules[i]);

	return block;
}

bool MatchBlock::Apply(const Block& block)
{
	Block copy = block;

	__try
	{
		reinterpret_cast<ApplyBlockFn>(CodeSignatures::Address(GameOffsets::kFnApplyMatchBlock))(copy.values);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}
