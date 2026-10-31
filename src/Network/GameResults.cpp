#include "Network/GameResults.h"

#include "Game/Engine/GameState.h"
#include "Game/Engine/OnlineState.h"
#include "Game/Lobby/OpponentLog.h"
#include "Game/Replays/LiveRecording.h"
#include "Game/Replays/ReplayFiles.h"
#include "Game/Replays/ReplayRecord.h"
#include "Network/GameEnding.h"
#include "Network/GameWinner.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/RoomRoster.h"
#include "Network/SideReadout.h"
#include "Network/Steam/SteamInterfaces.h"
#include "Palette/PaletteControl.h"

#include <Windows.h>

#include <cstdint>

namespace {

constexpr int kSides = 2;
constexpr int kMaxListeners = 4;
constexpr int kBattleOn = 1;
constexpr int kBattleTracked = 2;

GameWinner g_winner;
GameEnding g_ending;
GameResults::Report g_pending = {};

GameResults::Listener* g_listeners[kMaxListeners] = {};
int g_listenerCount = 0;

int g_battleState = 0;

struct Seat
{
	int side;
	uint64_t opponent;
};

Seat g_seat = { SideReadout::kFirstPlayer, 0 };

GameWinner::Side ReadSide(int player)
{
	return { SideReadout::ReadPattern(player), SideReadout::ReadVitals(player) };
}

bool IsPlayerSide(int side)
{
	return side == SideReadout::kFirstPlayer || side == SideReadout::kSecondPlayer;
}

bool IsGameRunning(bool inMatch)
{
	return inMatch && LiveRecording::IsRecording() && LiveRecording::RoundCount() > 0;
}

void LogBattleChange(bool inMatch, bool running, bool playing)
{
	const int state = (inMatch ? kBattleOn : 0) | (running ? kBattleTracked : 0);

	if (state == g_battleState)
		return;

	g_battleState = state;

	if (!playing)
		return;

	NetLog::Write("game: battle %s, recorder %s with %d round(s), tracked as %s", inMatch ? "on" : "off",
		LiveRecording::IsRecording() ? "recording" : "idle", LiveRecording::RoundCount(),
		running ? "running" : "not running");
}

int DecideWinner(bool playing, int local, uint64_t opponent, int frame)
{
	if (!playing)
	{
		g_winner.Reset();
		return GameWinner::kNoWinner;
	}

	const int winner = g_winner.Update(ReadSide(SideReadout::kFirstPlayer), ReadSide(SideReadout::kSecondPlayer));

	if (winner == GameWinner::kNoWinner)
		return winner;

	const bool won = winner == local;
	OpponentLog::RecordGame(opponent, won);

	NetLog::Write("game: side %d won, you are side %d (%s) at netplay frame %d", winner, local,
		won ? "win" : "loss", frame);

	return winner;
}

ReplayRecord::Facts FactsOf(const GameResults::Report& report)
{
	ReplayRecord::Facts facts = {};

	facts.version = static_cast<uint16_t>(ReplayFiles::CurrentVersion());
	facts.author = report.steamId[report.localSide];
	GetLocalTime(&facts.time);

	for (int side = 0; side < kSides; ++side)
		facts.player[side] = { report.steamId[side], report.chara[side] };

	return facts;
}

void Capture(const Seat& seat)
{
	g_pending = GameResults::Report();
	g_pending.localSide = seat.side;
	g_pending.steamId[seat.side] = SteamInterfaces::GetLocalSteamId();
	g_pending.steamId[1 - seat.side] = seat.opponent;

	for (int side = 0; side < kSides; ++side)
		g_pending.chara[side] = GameState::GetLoadedCharacter(side);

	std::vector<uint8_t> payload;

	if (!LiveRecording::Capture(payload))
	{
		NetLog::Write("game: the replay of this game could not be captured");
		return;
	}

	ReplayRecord::FromPayload(payload, FactsOf(g_pending), g_pending.record);
}

void Publish(const GameEnding::Step& step)
{
	g_pending.ending = step.ended == GameEnding::Cause::Decided ? GameResults::Ending::Decided
		: step.ended == GameEnding::Cause::OpponentLeft ? GameResults::Ending::OpponentLeft
		: GameResults::Ending::ConnectionLost;
	g_pending.winner = step.winner;

	NetLog::Write("game: ended, %s, winner side %d, replay %zu bytes", GameResults::EndingName(g_pending.ending),
		g_pending.winner, g_pending.record.size());

	for (int index = 0; index < g_listenerCount; ++index)
		g_listeners[index]->OnGameEnded(g_pending);

	g_pending.record.clear();
}

}

void GameResults::AddListener(Listener* listener)
{
	if (listener == nullptr || g_listenerCount >= kMaxListeners)
		return;

	g_listeners[g_listenerCount++] = listener;
}

const char* GameResults::EndingName(Ending ending)
{
	switch (ending)
	{
	case Ending::Decided:
		return "decided";
	case Ending::OpponentLeft:
		return "the opponent left";
	default:
		return "the connection was lost";
	}
}

void GameResults::OnFrame()
{
	const NetLink::Snapshot& link = NetLink::Current();

	if (OnlineState::IsSpectating())
	{
		g_winner.Reset();
		g_ending.Reset();
		return;
	}

	const int local = PaletteControl::LocalPlayer();
	const uint64_t opponent = OpponentLog::GetCurrentPeer();
	const bool playing = NetLink::InSession(link) && IsPlayerSide(local) && opponent != 0;
	const bool inMatch = GameState::IsInMatch();
	const bool running = IsGameRunning(inMatch);

	if (playing)
		g_seat = { local, opponent };

	LogBattleChange(inMatch, running, playing);

	const GameEnding::Frame frame = {
		playing,
		running,
		DecideWinner(playing, local, opponent, link.netplayFrame),
		g_seat.opponent != 0 ? RoomRoster::LastDepartureOf(g_seat.opponent) : 0,
		g_seat.side,
		GetTickCount()
	};

	const GameEnding::Step step = g_ending.Update(frame);

	if (step.captureNow)
		Capture(g_seat);

	if (step.ended != GameEnding::Cause::None)
		Publish(step);
}
