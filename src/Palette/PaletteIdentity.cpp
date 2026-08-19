#include "Palette/PaletteIdentity.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/Settings.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Palette/PaletteDrawProbe.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteTexture.h"
#include "Palette/PaletteTrace.h"

#include <cstring>

namespace {

constexpr int kSignatureFirstEntry = 2;
constexpr int kSignatureEntries = 16;

constexpr unsigned kRowsChecked = 4;

constexpr int kMinDistinctColours = 12;

constexpr int kIdentifyEveryFrames = 15;

struct Slot
{
	PaletteIdentity::Source source;
	int chara;
};

Slot g_slot[PaletteTexture::kMaxSeen] = {};
int g_generation = -1;
int g_identifyIn = 0;

struct Catalogue
{
	uint8_t signature[GameOffsets::kPaletteSlots][kSignatureEntries * 3];
	bool valid[GameOffsets::kPaletteSlots];
	int chara;
	int count;
	int passes;
};

constexpr int kCataloguePasses = 40;

Catalogue g_catalogue[2] = {};
bool g_coloursEverMatched = false;
int g_comparisons = 0;

int g_loggedAmbiguous[2] = {};

void TakeSignature(const uint8_t* palette, uint8_t* out)
{
	for (int i = 0; i < kSignatureEntries; ++i)
	{
		const int entry = kSignatureFirstEntry + i;
		out[i * 3 + 0] = palette[entry * 4 + 0];
		out[i * 3 + 1] = palette[entry * 4 + 1];
		out[i * 3 + 2] = palette[entry * 4 + 2];
	}
}

void RefreshCatalogue(int player)
{
	const int chara = PaletteManager::GetCharaNumber(player);
	if (chara < 0)
		return;

	Catalogue& catalogue = g_catalogue[player];

	if (catalogue.chara != chara)
	{
		catalogue.chara = chara;
		catalogue.count = 0;
		catalogue.passes = 0;
		memset(catalogue.valid, 0, sizeof(catalogue.valid));
	}
	else if (catalogue.count >= GameOffsets::kPaletteSlots || catalogue.passes >= kCataloguePasses)
	{
		return;
	}

	const int before = catalogue.count;
	++catalogue.passes;

	for (int i = 0; i < GameOffsets::kPaletteSlots; ++i)
	{
		if (catalogue.valid[i])
			continue;

		uint8_t palette[PaletteMemory::kPaletteBytes] = {};
		if (!PaletteMemory::ReadPlayerPaletteAt(player, i, palette))
			continue;

		TakeSignature(palette, catalogue.signature[i]);
		catalogue.valid[i] = true;
		++catalogue.count;
	}

	if (catalogue.count != before)
		LOG("identity: read %d of %d palettes for %s (P%d)", catalogue.count,
			GameOffsets::kPaletteSlots, PaletteManager::GetCharaName(chara), player + 1);
}

bool RowIsTellable(const uint8_t* row)
{
	int distinct = 1;

	for (int c = 1; c < 64 && distinct < kMinDistinctColours; ++c)
	{
		bool seen = false;
		for (int d = 0; d < c && !seen; ++d)
		{
			seen = row[c * 4 + 0] == row[d * 4 + 0] && row[c * 4 + 1] == row[d * 4 + 1] &&
				row[c * 4 + 2] == row[d * 4 + 2];
		}

		if (!seen)
			++distinct;
	}

	return distinct >= kMinDistinctColours;
}

bool RowMatchesCatalogue(const uint8_t* row, const Catalogue& catalogue, int& outPalette)
{
	uint8_t signature[kSignatureEntries * 3] = {};
	TakeSignature(row, signature);

	for (int i = 0; i < GameOffsets::kPaletteSlots; ++i)
	{
		if (!catalogue.valid[i])
			continue;

		++g_comparisons;

		if (memcmp(signature, catalogue.signature[i], sizeof(signature)) == 0)
		{
			outPalette = i;
			return true;
		}
	}

	return false;
}

void Claim(int index, uint8_t players, int chara, PaletteIdentity::Source source)
{
	if (index < 0 || players == 0)
		return;

	if (g_slot[index].source > source)
		return;

	const bool advisory = !g_modVals.paletteIdentifyByColours;

	const PaletteTexture::TextureOwner before = PaletteTexture::GetOwnerInfo(index);
	const bool changed = before.kind != PaletteTexture::OwnerKind::Player ||
		before.players != players || before.chara != static_cast<int16_t>(chara);

	if (!advisory)
	{
		PaletteTexture::TextureOwner owner = {};
		owner.kind = PaletteTexture::OwnerKind::Player;
		owner.players = players;
		owner.chara = static_cast<int16_t>(chara);

		PaletteTexture::AssignOwner(index, owner, PaletteTexture::Claim::Strong);
	}

	g_slot[index].source = source;
	g_slot[index].chara = chara;

	if (!changed)
		return;

	PaletteTrace::Note("colours say texture %d %s %s (%s), from %s", index,
		advisory ? "would be" : "is", PaletteManager::GetCharaName(chara),
		players == 0x3 ? "both sides" : (players == 0x1 ? "P1" : "P2"),
		PaletteIdentity::GetSourceName(source));

	LOG("identity: texture %d %s %s (%s), said by %s", index, advisory ? "would be" : "is",
		PaletteManager::GetCharaName(chara),
		players == 0x3 ? "both sides" : (players == 0x1 ? "P1" : "P2"),
		PaletteIdentity::GetSourceName(source));
}

bool ClaimFromCharaStack()
{
	if (PaletteDrawProbe::GetCharaHits() == 0)
		return false;

	void* const slots[2] = { MemoryMap::GetCharaSlot(0), MemoryMap::GetCharaSlot(1) };
	bool claimed = false;

	for (int t = 0; t < PaletteDrawProbe::GetTupleCount(); ++t)
	{
		PaletteDrawProbe::Tuple tuple = {};
		if (!PaletteDrawProbe::GetTuple(t, tuple) || tuple.chara == 0)
			continue;

		const int index = PaletteTexture::FindByPointer(tuple.texture);
		if (index < 0)
			continue;

		for (int player = 0; player < 2; ++player)
		{
			if (slots[player] == nullptr ||
				reinterpret_cast<uintptr_t>(slots[player]) != tuple.chara)
			{
				continue;
			}

			Claim(index, PaletteTexture::PlayerBit(player), PaletteManager::GetCharaNumber(player),
				PaletteIdentity::Source::CharaStack);
			claimed = true;
		}
	}

	return claimed;
}

void ClaimFromColours()
{
	int matchedTexture[2] = { -1, -1 };
	int matchedCount[2] = { 0, 0 };

	for (int index = 0; index < PaletteTexture::GetSeenCount(); ++index)
	{
		if (PaletteTexture::IsOwnerByHand(index))
			continue;

		if (g_slot[index].source >= PaletteIdentity::Source::Colours)
			continue;

		const PaletteTexture::OwnerKind kind = PaletteTexture::GetOwnerInfo(index).kind;
		if (kind == PaletteTexture::OwnerKind::CharSelect ||
			kind == PaletteTexture::OwnerKind::LobbyAvatar)
		{
			continue;
		}

		for (unsigned row = 0; row < kRowsChecked && row < PaletteTexture::kRows; ++row)
		{
			uint8_t colors[PaletteTexture::kRowBytes] = {};
			if (!PaletteTexture::ReadPristineRowAsRgba(index, row, colors))
				break;

			if (!RowIsTellable(colors))
				continue;

			bool done = false;

			for (int player = 0; player < 2 && !done; ++player)
			{
				const Catalogue& catalogue = g_catalogue[player];
				if (catalogue.count == 0)
					continue;

				int palette = -1;
				if (!RowMatchesCatalogue(colors, catalogue, palette))
					continue;

				if (!g_coloursEverMatched)
				{
					g_coloursEverMatched = true;
					LOG("identity: the colours match - texture %d row %u is palette %d of %s",
						index, row, palette, PaletteManager::GetCharaName(catalogue.chara));
				}

				matchedTexture[player] = index;
				++matchedCount[player];
				done = true;
			}

			if (done)
				break;
		}
	}

	for (int player = 0; player < 2; ++player)
	{
		if (matchedCount[player] == 0)
			continue;

		const Catalogue& catalogue = g_catalogue[player];

		if (matchedCount[player] > 1)
		{
			if (g_loggedAmbiguous[player] != matchedCount[player])
			{
				g_loggedAmbiguous[player] = matchedCount[player];
				LOG("identity: %d textures hold %s's colours - the colours name nobody",
					matchedCount[player], PaletteManager::GetCharaName(catalogue.chara));
			}

			continue;
		}

		g_loggedAmbiguous[player] = 0;

		Claim(matchedTexture[player], PaletteTexture::PlayerBit(player), catalogue.chara,
			PaletteIdentity::Source::Colours);
	}
}

void ClaimByElimination()
{
	int named = -1;
	int unnamed = -1;

	for (int player = 0; player < 2; ++player)
	{
		const int index = PaletteTexture::FindForPlayer(player);
		const bool strong = index >= 0 &&
			PaletteTexture::GetClaim(index) > PaletteTexture::Claim::Guess;

		if (strong)
			named = index;
		else
			unnamed = player;
	}

	if (named < 0 || unnamed < 0)
		return;

	if ((PaletteTexture::GetOwnerInfo(named).players & PaletteTexture::PlayerBit(unnamed)) != 0)
		return;

	const int chara = PaletteManager::GetCharaNumber(unnamed);
	if (chara < 0)
		return;

	int best = -1;

	for (int i = 0; i < PaletteTexture::GetSeenCount(); ++i)
	{
		if (i == named || !PaletteTexture::WasBoundInMatch(i))
			continue;

		if (PaletteTexture::GetClaim(i) > PaletteTexture::Claim::Guess)
			continue;

		const PaletteTexture::OwnerKind kind = PaletteTexture::GetOwnerInfo(i).kind;
		if (kind == PaletteTexture::OwnerKind::CharSelect ||
			kind == PaletteTexture::OwnerKind::LobbyAvatar)
		{
			continue;
		}

		if (best < 0 || PaletteTexture::GetFrameCount(i) > PaletteTexture::GetFrameCount(best))
			best = i;
	}

	if (best < 0 || PaletteTexture::GetFrameCount(best) < 30)
		return;

	Claim(best, PaletteTexture::PlayerBit(unnamed), chara, PaletteIdentity::Source::Elimination);
}

}

const char* PaletteIdentity::GetSourceName(Source source)
{
	switch (source)
	{
	case Source::Hand: return "hand";
	case Source::CharaStack: return "the chara stack";
	case Source::Colours: return "its colours";
	case Source::Elimination: return "elimination";
	case Source::Guess: return "the guess";
	default: return "nothing";
	}
}

PaletteIdentity::Source PaletteIdentity::GetSource(int textureIndex)
{
	if (textureIndex < 0 || textureIndex >= PaletteTexture::GetSeenCount())
		return Source::None;

	if (PaletteTexture::IsOwnerByHand(textureIndex))
		return Source::Hand;

	return g_slot[textureIndex].source;
}

int PaletteIdentity::GetChara(int textureIndex)
{
	if (textureIndex < 0 || textureIndex >= PaletteTexture::GetSeenCount())
		return -1;

	return g_slot[textureIndex].chara;
}

bool PaletteIdentity::ColoursEverMatched()
{
	return g_coloursEverMatched;
}

int PaletteIdentity::GetComparisons()
{
	return g_comparisons;
}

int PaletteIdentity::GetCatalogueCount(int player)
{
	if (player < 0 || player > 1)
		return 0;

	return g_catalogue[player].count;
}

void PaletteIdentity::Reset()
{
	memset(g_slot, 0, sizeof(g_slot));
	memset(g_catalogue, 0, sizeof(g_catalogue));
	memset(g_loggedAmbiguous, 0, sizeof(g_loggedAmbiguous));
	g_catalogue[0].chara = -1;
	g_catalogue[1].chara = -1;
	g_generation = -1;
	g_identifyIn = 0;
}

void PaletteIdentity::OnFrame()
{
	if (!GameState::IsInMatch())
		return;

	const int generation = PaletteTexture::GetGeneration();
	if (generation != g_generation)
	{
		g_generation = generation;
		memset(g_slot, 0, sizeof(g_slot));
		g_identifyIn = 0;
	}

	if (--g_identifyIn > 0)
		return;
	g_identifyIn = kIdentifyEveryFrames;

	RefreshCatalogue(0);
	RefreshCatalogue(1);

	ClaimFromCharaStack();
	ClaimFromColours();
	ClaimByElimination();

	int index[2] = { -1, -1 };
	bool strong[2] = { false, false };

	for (int player = 0; player < 2; ++player)
	{
		index[player] = PaletteTexture::FindForPlayer(player);
		strong[player] = index[player] >= 0 &&
			PaletteTexture::GetClaim(index[player]) > PaletteTexture::Claim::Guess;
	}

	const bool mirror = g_catalogue[0].chara >= 0 && g_catalogue[0].chara == g_catalogue[1].chara;
	const bool distinct = mirror ? index[0] == index[1] : index[0] != index[1];
	const bool takeOver = g_modVals.paletteIdentifyByColours && strong[0] && strong[1] && distinct;

	if (takeOver == PaletteTexture::IsHeuristicEnabled())
	{
		PaletteTexture::SetHeuristicEnabled(!takeOver);
		LOG("identity: %s", takeOver ? "both sides named - the guess is off"
			: "a side is unnamed - the guess is back on");
	}
}
