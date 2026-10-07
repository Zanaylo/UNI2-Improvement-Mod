#include "Network/SideReadout.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/MemoryMap.h"
#include "Game/Engine/PlayerHealth.h"

#include <cstdint>

RoundEvents::Vitals SideReadout::ReadVitals(int player)
{
	PlayerHealth::Health health = {};

	if (!PlayerHealth::Read(MemoryMap::GetCharaSlot(player), health))
		return {};

	return { health.current, health.max };
}

int SideReadout::ReadPattern(int player)
{
	const uint8_t* const chara = static_cast<const uint8_t*>(MemoryMap::GetCharaSlot(player));

	if (chara == nullptr)
		return kNoPattern;

	uint16_t pattern = 0;

	if (!TryReadMemory(&pattern, chara + GameOffsets::kPlayerDataPattern, sizeof(pattern)))
		return kNoPattern;

	return pattern;
}
