#include "Game/Engine/ComboLedger.h"

namespace {

constexpr int kPlayers = 2;
constexpr int kPercent = 100;

ComboLedger::Totals g_totals = { { false, 0, ComboProration::kFullRate, 0, ComboProration::kFullRate, 0, 0 } };
bool g_tracking = false;
int g_attacker = -1;

bool Active(int& outPlayer, ComboProration::Reading& out)
{
	for (int player = 0; player < kPlayers; ++player)
	{
		if (!ComboProration::Read(player, out) || !out.active)
			continue;

		outPlayer = player;
		return true;
	}

	return false;
}

int Undo(int damage, int rate)
{
	return rate > 0 ? damage * kPercent / rate - damage : 0;
}

int UndoBoth(int damage, int timeRate, int moveRate)
{
	if (timeRate <= 0 || moveRate <= 0)
		return 0;

	return damage * kPercent * kPercent / (timeRate * moveRate) - damage;
}

bool Started(int player, const ComboProration::Reading& now)
{
	if (!g_tracking || player != g_attacker)
		return true;

	const ComboProration::Reading& before = g_totals.reading;

	return now.damage < before.damage || now.hits < before.hits;
}

void Restart(int player)
{
	g_attacker = player;
	g_totals.timer = {};
	g_totals.moves = {};
	g_totals.comboLoss = 0;
	g_totals.reading.damage = 0;
	g_totals.reading.hits = 0;
}

void Record(int dealt, const ComboProration::Reading& now)
{
	g_totals.timer.lastHit = Undo(dealt, now.timeRate);
	g_totals.moves.lastHit = Undo(dealt, now.moveRate);
	g_totals.timer.combo += g_totals.timer.lastHit;
	g_totals.moves.combo += g_totals.moves.lastHit;
	g_totals.comboLoss += UndoBoth(dealt, now.timeRate, now.moveRate);
}

}

void ComboLedger::Sample()
{
	int player = -1;
	ComboProration::Reading now = {};

	if (!Active(player, now))
	{
		g_tracking = false;
		g_totals.reading.active = false;
		return;
	}

	if (Started(player, now))
		Restart(player);

	const int dealt = now.damage - g_totals.reading.damage;

	if (dealt > 0)
		Record(dealt, now);

	g_totals.reading = now;
	g_tracking = true;
}

const ComboLedger::Totals& ComboLedger::Current()
{
	return g_totals;
}
