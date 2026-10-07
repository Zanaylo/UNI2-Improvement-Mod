#include "Network/GameResults.h"

#include "Game/Engine/OnlineState.h"
#include "Game/Lobby/OpponentLog.h"
#include "Network/GameWinner.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/SideReadout.h"
#include "Palette/PaletteControl.h"

#include <cstdint>

namespace {

GameWinner g_winner;

GameWinner::Side ReadSide(int player)
{
	return { SideReadout::ReadPattern(player), SideReadout::ReadVitals(player) };
}

bool IsPlayerSide(int side)
{
	return side == SideReadout::kFirstPlayer || side == SideReadout::kSecondPlayer;
}

}

void GameResults::OnFrame()
{
	const NetLink::Snapshot& link = NetLink::Current();

	if (!NetLink::InSession(link) || OnlineState::IsSpectating())
	{
		g_winner.Reset();
		return;
	}

	const int local = PaletteControl::LocalPlayer();
	const uint64_t opponent = OpponentLog::GetCurrentPeer();

	if (!IsPlayerSide(local) || opponent == 0)
		return;

	const int winner = g_winner.Update(ReadSide(SideReadout::kFirstPlayer), ReadSide(SideReadout::kSecondPlayer));

	if (winner == GameWinner::kNoWinner)
		return;

	const bool won = winner == local;
	OpponentLog::RecordGame(opponent, won);

	NetLog::Write("game: side %d won, you are side %d (%s) at netplay frame %d", winner, local,
		won ? "win" : "loss", link.netplayFrame);
}
