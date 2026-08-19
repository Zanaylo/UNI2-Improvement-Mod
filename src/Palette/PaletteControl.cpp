#include "Palette/PaletteControl.h"

#include "Core/interfaces.h"
#include "Game/OnlineState.h"
#include "Network/PaletteShare.h"
#include "Palette/EffectPaint.h"
#include "Palette/PlayerSides.h"

namespace {

bool g_online = false;
bool g_spectating = false;
int g_local = -1;
bool g_wear[PaletteControl::kPlayers] = { true, true };
int g_playerForSide[PaletteControl::kPlayers] = { 0, 1 };

}

void PaletteControl::OnFrame()
{
	g_online = OnlineState::IsOnline();
	g_spectating = g_online && OnlineState::IsSpectating();

	for (int side = 0; side < kPlayers; ++side)
	{
		const int player = PlayerSides::PlayerOnScreenSide(side);

		if (player >= 0)
			g_playerForSide[side] = player;
	}

	const int seat = g_online && !g_spectating ? PaletteShare::GetOwnSide() : -1;

	g_local = seat >= 0 && seat < kPlayers ? seat : -1;

	for (int player = 0; player < kPlayers; ++player)
	{
		g_wear[player] = g_local < 0 || player == g_local
			? true : g_modVals.showOnlinePalettes;

		EffectPaint::SetWear(player, CanWear(player));
	}
}

bool PaletteControl::IsOnline()
{
	return g_online;
}

bool PaletteControl::IsSpectating()
{
	return g_spectating;
}

int PaletteControl::LocalPlayer()
{
	return g_local;
}

bool PaletteControl::CanEdit(int player)
{
	if (player < 0 || player >= kPlayers)
		return false;

	if (g_spectating)
		return false;

	if (g_local < 0)
		return true;

	return player == g_local;
}

const char* PaletteControl::WhyNot(int player)
{
	if (player < 0 || player >= kPlayers)
		return "";

	if (g_spectating)
		return "You are watching this match, so neither character's colours are yours to choose. "
			"What the players are wearing arrives from them.";

	if (g_local >= 0 && player != g_local)
		return "This is the other player's character. They choose its colours, and what they pick "
			"arrives here if they are running the mod.";

	return "";
}

int PaletteControl::PlayerForScreenSide(int side)
{
	return side >= 0 && side < kPlayers ? g_playerForSide[side] : -1;
}

bool PaletteControl::CanWear(int player)
{
	if (player < 0 || player >= kPlayers)
		return false;

	if (g_spectating)
		return g_modVals.showOnlinePalettes;

	return g_wear[player];
}
