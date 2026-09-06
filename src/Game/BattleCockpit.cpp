#include "Game/BattleCockpit.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"

#include <cstdint>

namespace {

bool g_reached = false;

void* Cockpit()
{
	const uintptr_t global = RvaToAddress(GameOffsets::kBattleCockpit);

	if (!IsAddressInGameModule(global))
		return nullptr;

	uint32_t object = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(global), object) || object == 0)
		return nullptr;

	if (!IsReadableMemory(reinterpret_cast<const void*>(object),
		GameOffsets::kCockpitView + sizeof(uint32_t)))
	{
		return nullptr;
	}

	return reinterpret_cast<void*>(object);
}

void Write(uint32_t view)
{
	void* const cockpit = Cockpit();

	if (cockpit == nullptr)
		return;

	void* const field = static_cast<uint8_t*>(cockpit) + GameOffsets::kCockpitView;

	uint32_t now = 0;

	if (TryReadDword(field, now) && now == view)
		return;

	if (TryWriteDword(field, view))
		g_reached = true;
}

}

bool BattleCockpit::IsHidden()
{
	return g_modVals.hideBattleHud != 0;
}

void BattleCockpit::SetHidden(bool hidden)
{
	g_modVals.hideBattleHud = hidden ? 1 : 0;
	Settings::SaveInt("Training", "HideHud", g_modVals.hideBattleHud);

	Write(hidden ? GameOffsets::kCockpitViewHidden : GameOffsets::kCockpitViewShown);
}

bool BattleCockpit::Reached()
{
	return g_reached;
}

void BattleCockpit::Update()
{
	if (!IsHidden())
		return;

	if (!GameState::IsInMatch())
		return;

	Write(GameOffsets::kCockpitViewHidden);
}
