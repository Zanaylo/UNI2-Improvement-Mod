#include "Palette/NetworkPickSeed.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Lobby/RoomSeat.h"
#include "Game/Tables/CharaTables.h"
#include "Palette/ColourSlots.h"
#include "Palette/NetworkPick.h"
#include "Palette/NetworkPickRule.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteSignature.h"

#include <cstdint>

namespace {

constexpr int kNone = -1;

NetworkPickRule::Profile g_reportedGame = { kNone, kNone };
NetworkPickRule::Profile g_reportedPick = { kNone, kNone };

int ReadProfileField(uintptr_t field)
{
	int value = kNone;
	TryReadMemory(&value, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kNetworkProfile) + field), sizeof(value));

	return value;
}

NetworkPickRule::Profile ReadGameProfile()
{
	return { ReadProfileField(GameOffsets::kNetworkProfileChara), ReadProfileField(GameOffsets::kNetworkProfileColour) };
}

NetworkPickRule::Pick Describe(const NetworkPick::Pick& pick, bool present)
{
	return { present, pick.chara, pick.colour, present && pick.hasSignature };
}

bool Same(const NetworkPickRule::Profile& a, const NetworkPickRule::Profile& b)
{
	return a.chara == b.chara && a.colour == b.colour;
}

void ReportDisagreement(const NetworkPickRule::Pick& pick, const NetworkPickRule::Profile& game,
	const NetworkPickRule::Limits& limits)
{
	const NetworkPickRule::Profile mine = { pick.chara, pick.colour };

	if (!NetworkPickRule::Disagrees(pick, game, limits) || (Same(mine, g_reportedPick) && Same(game, g_reportedGame)))
		return;

	g_reportedPick = mine;
	g_reportedGame = game;
	LOG("network pick: keeping character %d colour %d, the game's network profile says character %d colour %d",
		mine.chara, mine.colour, game.chara, game.colour);
}

void Adopt(const NetworkPickRule::Profile& game)
{
	NetworkPick::Remember(game.chara, game.colour, "");
	LOG("network pick: none confirmed with the mod, taking the game's network profile, character %d colour %d",
		game.chara, game.colour);
}

void Sign(const NetworkPick::Pick& pick, int seat)
{
	uint8_t shown[PalettePaint::kBytes] = {};

	if (!PalettePaint::ReadGameColours(seat, shown))
		return;

	uint8_t signature[PaletteSignature::kBytes] = {};
	PaletteSignature::Take(shown, signature);

	NetworkPick::Remember(pick.chara, pick.colour, pick.file, signature);
	LOG("network pick: character %d colour %d signed from the room avatar in seat %d", pick.chara, pick.colour,
		seat + 1);
}

}

void NetworkPickSeed::OnFrame()
{
	NetworkPick::Pick pick = { kNone, kNone, "", false, {} };
	const bool present = NetworkPick::Recall(pick);

	const NetworkPickRule::Profile game = ReadGameProfile();
	const NetworkPickRule::Limits limits = { CharaTables::GetCharaCount(), ColourSlots::kGameColours };
	const int seat = RoomSeat::Local();
	const NetworkPickRule::Pick described = Describe(pick, present);

	switch (NetworkPickRule::Decide(described, game, limits, seat != RoomSeat::kNone))
	{
	case NetworkPickRule::Action::Adopt:
		Adopt(game);
		return;
	case NetworkPickRule::Action::Sign:
		Sign(pick, seat);
		return;
	case NetworkPickRule::Action::Keep:
		ReportDisagreement(described, game, limits);
		return;
	}
}
