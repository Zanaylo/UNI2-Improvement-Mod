#include "Game/Lobby/PersonaNameHook.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Lobby/KnownNames.h"
#include "Game/Lobby/NameCensor.h"
#include "Hooks/GameHook.h"

#include <cstdint>

namespace {

typedef void(__fastcall* ResolveNameFn)(const uint64_t*, char*);

GameHook<ResolveNameFn> g_resolveNameHook("ResolvePersonaName");

void __fastcall HookedResolveName(const uint64_t* id, char* out)
{
	g_resolveNameHook.Original()(id, out);

	if (id == nullptr || out == nullptr)
		return;

	KnownNames::Fill(*id, out, GameOffsets::kResolvedNameBytes);
	NameCensor::Apply(*id, out);
}

}

bool PersonaNameHook::Install()
{
	if (g_resolveNameHook.IsLive())
		return true;

	void* const target = reinterpret_cast<void*>(CodeSignatures::Address(GameOffsets::kFnResolvePersonaName));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)) ||
		!g_resolveNameHook.Install(target, &HookedResolveName))
	{
		LOG("PersonaNameHook: the game's player name code is not where this game version expects it");
		return false;
	}

	LOG("PersonaNameHook: player names go through the mod");
	return true;
}
