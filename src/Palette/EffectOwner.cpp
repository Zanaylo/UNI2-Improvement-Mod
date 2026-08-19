#include "Palette/EffectOwner.h"

#include "Game/EffectTable.h"
#include "Game/PartColourTable.h"
#include "Game/StockPalettes.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteSeat.h"
#include "Palette/PaletteTexture.h"
#include "Palette/PlayerSides.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int kPlayers = EffectOwner::kPlayers;
constexpr int kEntries = EffectOwner::kEntries;

constexpr int kMaxStock = 48;

constexpr int kWornFreshFrames = 30;

constexpr int kTolerance = 1;

struct Entry
{
	uint8_t worn[kPlayers][3];
	uint8_t stock[kPlayers][kMaxStock][3];
	uint8_t stockCount[kPlayers];
	uint8_t claims;
};

Entry g_entries[kEntries] = {};

bool g_wornOk[kPlayers] = {};
int g_wornStale[kPlayers] = {};
bool g_mirror = false;

int g_chara[kPlayers] = { -1, -1 };
int g_stockRows[kPlayers] = {};
int g_claimCount[kPlayers] = {};

int g_wornFrame = -kWornFreshFrames;
int g_wornIndex[kPlayers] = { -2, -2 };
int g_wornSide[kPlayers] = { -2, -2 };
unsigned g_wornGeneration = 0;

struct Decision
{
	uint32_t colour;
	unsigned generation;
	int player;
	int route;
};

Decision g_decisions[kEntries] = {};
unsigned g_generation = 1;

uint32_t Pack(uint8_t r, uint8_t g, uint8_t b)
{
	return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

int g_byWorn = 0;
int g_byStock = 0;
int g_byClaim = 0;
int g_ambiguous = 0;
int g_unresolved = 0;
int g_soleWanters = 0;

char g_description[256] = "nothing taken yet";

bool Same(const uint8_t* a, uint8_t r, uint8_t g, uint8_t b)
{
	const int dr = static_cast<int>(a[0]) - r;
	const int dg = static_cast<int>(a[1]) - g;
	const int db = static_cast<int>(a[2]) - b;

	return dr <= kTolerance && dr >= -kTolerance
		&& dg <= kTolerance && dg >= -kTolerance
		&& db <= kTolerance && db >= -kTolerance;
}

void AddStock(int player, int entry, const uint8_t* rgb)
{
	Entry& at = g_entries[entry];

	if (at.stockCount[player] >= kMaxStock)
		return;

	for (int i = 0; i < at.stockCount[player]; ++i)
	{
		if (Same(at.stock[player][i], rgb[0], rgb[1], rgb[2]))
			return;
	}

	memcpy(at.stock[player][at.stockCount[player]], rgb, 3);
	++at.stockCount[player];
}

void TakeCharacter(int player, int chara)
{
	const uint8_t bit = static_cast<uint8_t>(1u << player);

	for (Entry& at : g_entries)
	{
		at.stockCount[player] = 0;
		at.claims = static_cast<uint8_t>(at.claims & ~bit);
	}

	g_stockRows[player] = 0;
	g_claimCount[player] = 0;

	if (chara < 0)
		return;

	if (StockPalettes::Load(chara))
	{
		g_stockRows[player] = StockPalettes::GetCount(chara);

		for (int palette = 0; palette < g_stockRows[player]; ++palette)
		{
			const uint8_t* const row = StockPalettes::GetRow(chara, palette);

			if (row == nullptr)
				continue;

			for (int entry = 1; entry < kEntries; ++entry)
				AddStock(player, entry, row + entry * 4);
		}
	}

	for (int entry = 1; entry < kEntries; ++entry)
	{
		if (!PartColourTable::IsPartEntry(chara, entry)
			&& EffectTable::CountUsing(chara, entry) <= 0)
		{
			continue;
		}

		g_entries[entry].claims = static_cast<uint8_t>(g_entries[entry].claims | bit);
		++g_claimCount[player];
	}
}

int OnlyClaimer(const Entry& at)
{
	const bool first = (at.claims & 1u) != 0;
	const bool second = (at.claims & 2u) != 0;

	if (first == second)
		return -1;

	return first ? 0 : 1;
}

int FromWorn(const Entry& at, uint8_t r, uint8_t g, uint8_t b, int& outMatches)
{
	int found = -1;
	outMatches = 0;

	for (int player = 0; player < kPlayers; ++player)
	{
		if (!g_wornOk[player] || !Same(at.worn[player], r, g, b))
			continue;

		found = player;
		++outMatches;
	}

	return found;
}

int FromStock(const Entry& at, uint8_t r, uint8_t g, uint8_t b, int& outMatches)
{
	int found = -1;
	outMatches = 0;

	for (int player = 0; player < kPlayers; ++player)
	{
		bool holds = false;

		for (int i = 0; i < at.stockCount[player] && !holds; ++i)
			holds = Same(at.stock[player][i], r, g, b);

		if (!holds)
			continue;

		found = player;
		++outMatches;
	}

	return found;
}

int Answer(int player, EffectOwner::Route route, int& outRoute)
{
	outRoute = static_cast<int>(route);
	return player;
}

int Decide(int entry, uint8_t r, uint8_t g, uint8_t b, int& outRoute)
{
	const Entry& at = g_entries[entry];
	const int onlyClaimer = OnlyClaimer(at);

	int matches = 0;
	int found = FromWorn(at, r, g, b, matches);

	if (matches == 1)
		return Answer(found, EffectOwner::Route::Worn, outRoute);

	if (matches == 0)
	{
		found = FromStock(at, r, g, b, matches);

		if (matches == 1)
			return Answer(found, EffectOwner::Route::Stock, outRoute);
	}

	if (matches > 1 && onlyClaimer < 0)
		return Answer(-1, EffectOwner::Route::Ambiguous, outRoute);

	if (onlyClaimer >= 0)
		return Answer(onlyClaimer, EffectOwner::Route::Claim, outRoute);

	return Answer(-1, EffectOwner::Route::None, outRoute);
}

void Count(int route)
{
	switch (static_cast<EffectOwner::Route>(route))
	{
	case EffectOwner::Route::Worn: ++g_byWorn; break;
	case EffectOwner::Route::Stock: ++g_byStock; break;
	case EffectOwner::Route::Claim: ++g_byClaim; break;
	case EffectOwner::Route::Ambiguous: ++g_ambiguous; break;
	default: ++g_unresolved; break;
	}
}

void TakeWorn(int player)
{
	uint8_t row[PalettePaint::kBytes] = {};

	if (!PalettePaint::ReadGameColours(player, row))
	{
		++g_wornStale[player];
		return;
	}

	for (int entry = 0; entry < kEntries; ++entry)
		memcpy(g_entries[entry].worn[player], row + entry * 4, 3);

	g_wornOk[player] = true;
	g_wornStale[player] = 0;
	++g_generation;
}

}

void EffectOwner::OnFrame()
{
	for (int player = 0; player < kPlayers; ++player)
	{
		const int chara = PaletteMemory::GetCharaNumber(player);

		if (chara == g_chara[player])
			continue;

		g_chara[player] = chara;
		TakeCharacter(player, chara);

		g_wornOk[player] = false;
		g_wornIndex[player] = -2;
		++g_generation;
	}

	g_mirror = g_chara[0] >= 0 && g_chara[0] == g_chara[1];

	const int frame = PaletteSeat::GetFrame();
	const unsigned generation = static_cast<unsigned>(PaletteTexture::GetGeneration());

	int index[kPlayers] = {};
	int side[kPlayers] = {};
	bool due = frame - g_wornFrame >= kWornFreshFrames || generation != g_wornGeneration;

	for (int player = 0; player < kPlayers; ++player)
	{
		index[player] = PalettePaint::GetIndex(player);

		side[player] = PaletteSeat::GetSideByOwner(PalettePaint::GetOwner(player));

		if (side[player] < 0)
			side[player] = PlayerSides::ScreenSideOf(player);

		if (index[player] != g_wornIndex[player] || side[player] != g_wornSide[player])
			due = true;
	}

	if (!due)
		return;

	g_wornFrame = frame;
	g_wornGeneration = generation;

	for (int player = 0; player < kPlayers; ++player)
	{
		g_wornIndex[player] = index[player];
		g_wornSide[player] = side[player];

		TakeWorn(player);
	}

	sprintf_s(g_description, "p1 chara %d, %d stock rows, %d claimed, worn %s%s | "
		"p2 chara %d, %d stock rows, %d claimed, worn %s%s%s",
		g_chara[0], g_stockRows[0], g_claimCount[0],
		g_wornOk[0] ? "read" : "NEVER read", g_wornStale[0] > 0 ? " (stale)" : "",
		g_chara[1], g_stockRows[1], g_claimCount[1],
		g_wornOk[1] ? "read" : "NEVER read", g_wornStale[1] > 0 ? " (stale)" : "",
		g_mirror ? " | mirror" : "");
}

int EffectOwner::PlayerFor(int entry, uint8_t r, uint8_t g, uint8_t b, Route& outRoute)
{
	if (entry <= 0 || entry >= kEntries)
	{
		outRoute = Route::None;
		++g_unresolved;
		return -1;
	}

	const uint32_t colour = Pack(r, g, b);
	Decision& decided = g_decisions[entry];

	if (decided.generation != g_generation || decided.colour != colour)
	{
		decided.colour = colour;
		decided.generation = g_generation;
		decided.player = Decide(entry, r, g, b, decided.route);
	}

	outRoute = static_cast<Route>(decided.route);
	Count(decided.route);

	return decided.player;
}

bool EffectOwner::Claims(int player, int entry)
{
	if (player < 0 || player >= kPlayers || entry <= 0 || entry >= kEntries)
		return false;

	return (g_entries[entry].claims & (1u << player)) != 0;
}

bool EffectOwner::IsMirror()
{
	return g_mirror;
}

bool EffectOwner::GetWorn(int player, int entry, uint8_t* outRgb)
{
	if (player < 0 || player >= kPlayers || entry < 0 || entry >= kEntries || !g_wornOk[player])
		return false;

	if (outRgb != nullptr)
		memcpy(outRgb, g_entries[entry].worn[player], 3);

	return true;
}

int EffectOwner::GetStockCount(int player, int entry)
{
	if (player < 0 || player >= kPlayers || entry < 0 || entry >= kEntries)
		return 0;

	return g_entries[entry].stockCount[player];
}

int EffectOwner::FindStock(int player, int entry, uint8_t r, uint8_t g, uint8_t b)
{
	if (player < 0 || player >= kPlayers || entry < 0 || entry >= kEntries)
		return -1;

	const Entry& at = g_entries[entry];

	for (int i = 0; i < at.stockCount[player]; ++i)
	{
		if (Same(at.stock[player][i], r, g, b))
			return i;
	}

	return -1;
}

void EffectOwner::GetCounts(int& outWorn, int& outStock, int& outClaim, int& outAmbiguous,
	int& outUnresolved)
{
	outWorn = g_byWorn;
	outStock = g_byStock;
	outClaim = g_byClaim;
	outAmbiguous = g_ambiguous;
	outUnresolved = g_unresolved;
}

void EffectOwner::NoteSoleWanter()
{
	++g_soleWanters;
}

int EffectOwner::GetSoleWanters()
{
	return g_soleWanters;
}

void EffectOwner::ResetCounts()
{
	++g_generation;

	g_byWorn = 0;
	g_byStock = 0;
	g_byClaim = 0;
	g_ambiguous = 0;
	g_unresolved = 0;
	g_soleWanters = 0;
}

const char* EffectOwner::Describe()
{
	return g_description;
}
