#include "Palette/PaletteBinder.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/Settings.h"
#include "Game/GameState.h"
#include "Palette/PaletteDrawProbe.h"
#include "Palette/PaletteManager.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PaletteTexture.h"
#include "Palette/PaletteTrace.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kFreshFrames = 20;

constexpr int kCorrelateFrames = 300;
constexpr int kAttributeEveryFrames = 30;
constexpr int kNameEveryFrames = 15;
constexpr unsigned kSeatVotesNeeded = 60;
constexpr unsigned kSeatMajority = 4;

constexpr int kCharaCount = 27;
constexpr int kSignatureEntries = 16;
constexpr int kSignatureBytes = kSignatureEntries * 3;

constexpr int kSignatureFirstEntry = 2;

constexpr int kSignatureRows = 8;

uint8_t g_signature[kCharaCount][kSignatureBytes] = {};
bool g_hasSignature[kCharaCount] = {};
bool g_signaturesLoaded = false;

void LoadSignatures()
{
	if (g_signaturesLoaded)
		return;

	g_signaturesLoaded = true;

	for (int chara = 0; chara < kCharaCount; ++chara)
	{
		char key[16] = {};
		sprintf_s(key, "%d", chara);

		char text[kSignatureBytes * 2 + 2] = {};
		GetPrivateProfileStringA("PaletteSignatures", key, "", text, sizeof(text),
			Settings::GetIniPath().c_str());

		if (strlen(text) != kSignatureBytes * 2)
			continue;

		bool good = true;
		for (int i = 0; i < kSignatureBytes && good; ++i)
		{
			unsigned value = 0;
			if (sscanf_s(text + i * 2, "%2x", &value) != 1)
				good = false;
			else
				g_signature[chara][i] = static_cast<uint8_t>(value);
		}

		g_hasSignature[chara] = good;
	}
}

void LearnSignature(int chara, const uint8_t* palette)
{
	if (chara < 0 || chara >= kCharaCount || palette == nullptr || g_hasSignature[chara])
		return;

	for (int i = 0; i < kSignatureEntries; ++i)
	{
		const int entry = kSignatureFirstEntry + i;
		g_signature[chara][i * 3 + 0] = palette[entry * 4 + 0];
		g_signature[chara][i * 3 + 1] = palette[entry * 4 + 1];
		g_signature[chara][i * 3 + 2] = palette[entry * 4 + 2];
	}

	g_hasSignature[chara] = true;

	char text[kSignatureBytes * 2 + 2] = {};
	for (int i = 0; i < kSignatureBytes; ++i)
		sprintf_s(text + i * 2, sizeof(text) - i * 2, "%02x", g_signature[chara][i]);

	char key[16] = {};
	sprintf_s(key, "%d", chara);
	WritePrivateProfileStringA("PaletteSignatures", key, text, Settings::GetIniPath().c_str());

	LOG("binder: learned the colours of %s", PaletteManager::GetCharaName(chara));
}

void LearnFromTexture(int index, int chara)
{
	if (chara < 0 || chara >= kCharaCount || g_hasSignature[chara])
		return;

	uint8_t row[PaletteTexture::kRowBytes] = {};
	if (PaletteTexture::ReadPristineRowAsRgba(index, 0, row))
		LearnSignature(chara, row);
}

bool CharaForRow(const uint8_t* row, int& outChara)
{
	for (int chara = 0; chara < kCharaCount; ++chara)
	{
		if (!g_hasSignature[chara])
			continue;

		bool same = true;
		for (int i = 0; i < kSignatureEntries && same; ++i)
		{
			const int entry = kSignatureFirstEntry + i;
			same = row[entry * 4 + 0] == g_signature[chara][i * 3 + 0] &&
				row[entry * 4 + 1] == g_signature[chara][i * 3 + 1] &&
				row[entry * 4 + 2] == g_signature[chara][i * 3 + 2];
		}

		if (same)
		{
			outChara = chara;
			return true;
		}
	}

	return false;
}

bool CharaForTexture(int index, int& outChara)
{
	for (unsigned row = 0; row < kSignatureRows && row < PaletteTexture::kRows; ++row)
	{
		uint8_t colors[PaletteTexture::kRowBytes] = {};
		if (!PaletteTexture::ReadPristineRowAsRgba(index, row, colors))
			return false;

		if (CharaForRow(colors, outChara))
			return true;
	}

	return false;
}

int g_lastGeneration = -1;
int g_lastSeenCount = -1;
bool g_lastInMatch = false;
bool g_lastWantCapture = false;
int g_attributeIn = 0;

uintptr_t g_observed[2] = {};
int g_nameIn = 0;

int g_loggedBinder[2] = { -1, -1 };
int g_loggedGuess[2] = { -1, -1 };

bool g_wasAuthoritative = false;

bool g_haveObservation = false;

void ObserveCharacterTextures(uintptr_t outTexture[2])
{
	outTexture[0] = 0;
	outTexture[1] = 0;

	int bestAge[2] = { kFreshFrames, kFreshFrames };

	const int count = PaletteDrawProbe::GetTupleCount();
	for (int i = 0; i < count; ++i)
	{
		PaletteDrawProbe::Tuple tuple = {};
		if (!PaletteDrawProbe::GetTuple(i, tuple))
			continue;

		if (!tuple.mainPath || (tuple.row != 0 && tuple.row != 1))
			continue;

		if (tuple.lastAgeFrames >= bestAge[tuple.row])
			continue;

		if (PaletteTexture::FindByPointer(tuple.texture) < 0)
			continue;

		bestAge[tuple.row] = tuple.lastAgeFrames;
		outTexture[tuple.row] = tuple.texture;
	}
}

void CompareWithGuess()
{
	for (int player = 0; player < 2; ++player)
	{
		const int binder = PaletteTexture::FindByPointer(g_observed[player]);
		const int guess = PaletteTexture::FindForPlayer(player);

		if (binder < 0)
			continue;

		if (binder == g_loggedBinder[player] && guess == g_loggedGuess[player])
			continue;

		g_loggedBinder[player] = binder;
		g_loggedGuess[player] = guess;

		if (binder != guess)
			LOG("binder: draws say P%d is texture %d, the guess says %d", player + 1, binder,
				guess);
	}
}

bool SidesAreStrong()
{
	for (int player = 0; player < 2; ++player)
	{
		const int index = PaletteTexture::FindForPlayer(player);
		if (index < 0 || PaletteTexture::GetClaim(index) == PaletteTexture::Claim::Guess)
			return false;
	}

	return true;
}

bool SameColours(const uint8_t* a, const uint8_t* b)
{
	for (int c = 0; c < PaletteTexture::kRowBytes; c += 4)
	{
		if (a[c] != b[c] || a[c + 1] != b[c + 1] || a[c + 2] != b[c + 2])
			return false;
	}

	return true;
}

int PurestForSeat(int seat)
{
	int best = -1;
	unsigned bestFor = 0;
	unsigned bestAgainst = 0;

	for (int i = 0; i < PaletteTexture::GetSeenCount(); ++i)
	{
		unsigned votes[2] = {};
		if (!PaletteDrawProbe::GetSeatVotes(PaletteTexture::GetSeen(i), votes[0], votes[1]))
			continue;

		const unsigned mine = votes[seat];
		const unsigned other = votes[1 - seat];

		if (mine < kSeatVotesNeeded || mine < other * kSeatMajority)
			continue;

		if (best >= 0 && mine * (bestAgainst + 1) <= bestFor * (other + 1))
			continue;

		best = i;
		bestFor = mine;
		bestAgainst = other;
	}

	return best;
}

int MatchTextureLike(int source, int first, int second)
{
	uint8_t wanted[PaletteTexture::kRowBytes] = {};
	if (!PaletteTexture::ReadPristineRowAsRgba(source, 0, wanted))
		return -1;

	uint8_t colours[PaletteTexture::kRowBytes] = {};

	const bool hitsFirst = PaletteTexture::ReadPristineRowAsRgba(first, 0, colours) &&
		SameColours(wanted, colours);

	const bool hitsSecond = PaletteTexture::ReadPristineRowAsRgba(second, 0, colours) &&
		SameColours(wanted, colours);

	if (hitsFirst == hitsSecond)
		return -1;

	return hitsFirst ? first : second;
}

bool g_namedByDraws = false;

void Adopt(const int wanted[2], const char* how, bool byDraws)
{
	const bool swap = PaletteTexture::GetSwapSides();
	const int target[2] = { wanted[swap ? 1 : 0], wanted[swap ? 0 : 1] };

	bool changed = false;

	for (int player = 0; player < 2; ++player)
	{
		if (PaletteTexture::IsOwnerByHand(target[player]))
			return;

		if (PaletteTexture::FindForPlayer(player) != target[player] ||
			PaletteTexture::GetClaim(target[player]) == PaletteTexture::Claim::Guess)
		{
			changed = true;
		}
	}

	if (!changed)
		return;

	for (int player = 0; player < 2; ++player)
	{
		const int current = PaletteTexture::FindForPlayer(player);
		if (current >= 0 && current != target[player])
			PaletteTexture::RestoreAll(current);
	}

	const uintptr_t pointer[2] = {
		PaletteTexture::GetSeen(target[0]), PaletteTexture::GetSeen(target[1])
	};

	for (int player = 0; player < 2; ++player)
	{
		const int index = PaletteTexture::FindByPointer(pointer[player]);
		if (index < 0)
			continue;

		PaletteTexture::TextureOwner owner = {};
		owner.kind = PaletteTexture::OwnerKind::Player;
		owner.players = PaletteTexture::PlayerBit(player);
		owner.chara = static_cast<int16_t>(PaletteManager::GetCharaNumber(player));

		PaletteTexture::AssignOwner(index, owner, PaletteTexture::Claim::Strong);
		LearnFromTexture(index, PaletteManager::GetCharaNumber(player));
	}

	g_namedByDraws = g_namedByDraws || byDraws;

	PaletteTrace::Note("%s gives P1 texture %d and P2 texture %d%s", how, target[0], target[1],
		swap ? ", sides swapped" : "");

	PaletteManager::RepaintSides();
}

void NameSidesFromSignatures(int first, int second)
{
	int named[2] = { -1, -1 };
	if (!CharaForTexture(first, named[0]) || !CharaForTexture(second, named[1]))
		return;

	if (named[0] == named[1])
		return;

	int wanted[2] = { -1, -1 };

	for (int seat = 0; seat < 2; ++seat)
	{
		const int chara = PaletteMemory::GetCharaNumber(seat);
		if (chara < 0)
			return;

		wanted[seat] = named[0] == chara ? first : (named[1] == chara ? second : -1);
	}

	if (wanted[0] < 0 || wanted[1] < 0 || wanted[0] == wanted[1])
		return;

	Adopt(wanted, "the colours we learned", false);
}

void NameSidesFromDraws()
{
	if (g_namedByDraws)
		return;

	int first = -1;
	int second = -1;
	if (!PaletteTexture::GetMatchTextures(first, second))
		return;

	if (!SidesAreStrong())
		NameSidesFromSignatures(first, second);

	int wanted[2] = { -1, -1 };

	for (int seat = 0; seat < 2; ++seat)
	{
		const int purest = PurestForSeat(seat);
		if (purest < 0)
			continue;

		wanted[seat] = MatchTextureLike(purest, first, second);
	}

	if (wanted[0] < 0 && wanted[1] >= 0)
		wanted[0] = wanted[1] == first ? second : first;
	else if (wanted[1] < 0 && wanted[0] >= 0)
		wanted[1] = wanted[0] == first ? second : first;

	if (wanted[0] < 0 || wanted[1] < 0 || wanted[0] == wanted[1])
		return;

	Adopt(wanted, "the draws", true);
}

void AttributeOutOfMatchTextures()
{
	LoadSignatures();

	const int loadCount = PaletteMemory::GetLoadCount();

	for (int i = PaletteTexture::GetSeenCount() - 1; i >= 0; --i)
	{
		if (PaletteTexture::WasBoundInMatch(i) || PaletteTexture::IsOwnerByHand(i))
			continue;

		if (PaletteTexture::GetOwnerInfo(i).kind != PaletteTexture::OwnerKind::Unknown)
			continue;

		int chara = -1;
		if (!CharaForTexture(i, chara))
			continue;

		PaletteTexture::TextureOwner owner = {};
		owner.kind = PaletteTexture::OwnerKind::CharSelect;
		owner.chara = static_cast<int16_t>(chara);
		PaletteTexture::AssignOwner(i, owner, PaletteTexture::Claim::Strong);

		LOG("binder: texture %d is a screen's, its colours are %s", i,
			PaletteManager::GetCharaName(chara));
	}

	int unowned = -1;
	int unownedCount = 0;

	for (int i = 0; i < PaletteTexture::GetSeenCount(); ++i)
	{
		if (PaletteTexture::WasBoundInMatch(i) || PaletteTexture::IsOwnerByHand(i))
			continue;

		if (PaletteTexture::GetOwnerInfo(i).kind != PaletteTexture::OwnerKind::Unknown)
			continue;

		unowned = i;
		++unownedCount;
	}

	if (unownedCount == 1)
	{
		int chara = -1;
		for (int player = 0; player < 2 && chara < 0; ++player)
		{
			const int number = PaletteMemory::GetCharaNumber(player);
			if (number >= 0 && number < kCharaCount)
				chara = number;
		}

		if (chara >= 0)
		{
			PaletteTexture::TextureOwner owner = {};
			owner.kind = PaletteTexture::OwnerKind::CharSelect;
			owner.chara = static_cast<int16_t>(chara);
			PaletteTexture::AssignOwner(unowned, owner, PaletteTexture::Claim::Strong);

			LOG("binder: texture %d is the screen's only one and the game says %s", unowned,
				PaletteManager::GetCharaName(chara));

			if (!g_hasSignature[chara] && !PaletteTexture::HasBackup(unowned, 0))
			{
				uint8_t row[PaletteTexture::kRowBytes] = {};
				if (PaletteTexture::ReadRowAsRgba(unowned, 0, row))
					LearnSignature(chara, row);
			}
		}
	}

	if (loadCount == 0)
		return;

	for (int i = PaletteTexture::GetSeenCount() - 1; i >= 0; --i)
	{
		if (PaletteTexture::WasBoundInMatch(i) || PaletteTexture::IsOwnerByHand(i))
			continue;

		if (PaletteTexture::GetOwnerInfo(i).kind != PaletteTexture::OwnerKind::Unknown)
			continue;

		const int seenAt = PaletteTexture::GetFirstSeenFrame(i);

		uint8_t row0[PaletteTexture::kRowBytes] = {};
		bool haveRow = PaletteTexture::ReadRowAsRgba(i, 0, row0);

		if (haveRow)
		{
			int distinct = 1;
			for (int c = 4; c < PaletteTexture::kRowBytes && distinct < 16; c += 4)
			{
				bool seenBefore = false;
				for (int d = 0; d < c && !seenBefore; d += 4)
				{
					seenBefore = row0[c] == row0[d] && row0[c + 1] == row0[d + 1] &&
						row0[c + 2] == row0[d + 2];
				}

				if (!seenBefore)
					++distinct;
			}

			haveRow = distinct >= 16;
		}

		int nearest = -1;
		int nearestDistance = kCorrelateFrames + 1;
		int nearbyCount = 0;
		int matched = -1;

		for (int load = 0; load < loadCount && matched < 0; ++load)
		{
			const int chara = PaletteMemory::GetLoadChara(load);

			if (chara < 0)
				continue;

			const int frame = PaletteMemory::GetLoadFrame(load);
			const int distance = seenAt >= frame ? seenAt - frame : frame - seenAt;

			if (distance <= kCorrelateFrames)
			{
				++nearbyCount;

				if (distance < nearestDistance)
				{
					nearestDistance = distance;
					nearest = load;
				}
			}

			if (!haveRow)
				continue;

			int palettes = PaletteMemory::GetLoadPaletteCount(load);
			if (palettes > 45)
				palettes = 45;

			for (int p = 0; p < palettes && matched < 0; ++p)
			{
				uint8_t palette[PaletteMemory::kPaletteBytes] = {};
				if (!PaletteMemory::ReadPalette(load, p, palette))
					break;

				bool same = true;
				for (int c = 0; c < PaletteTexture::kRowBytes && same; c += 4)
				{
					same = row0[c] == palette[c] && row0[c + 1] == palette[c + 1] &&
						row0[c + 2] == palette[c + 2];
				}

				if (same)
					matched = load;
			}
		}

		const int chosen = matched >= 0 ? matched : (nearbyCount == 1 ? nearest : -1);
		if (chosen < 0)
			continue;

		PaletteTexture::TextureOwner owner = {};
		owner.kind = PaletteMemory::GetLoadKind(chosen) == PaletteMemory::kLoadLobbyAvatar
			? PaletteTexture::OwnerKind::LobbyAvatar
			: PaletteTexture::OwnerKind::CharSelect;
		owner.chara = static_cast<int16_t>(PaletteMemory::GetLoadChara(chosen));
		PaletteTexture::AssignOwner(i, owner, PaletteTexture::Claim::Strong);

		LOG("binder: texture %d is '%s', matched by %s", i, PaletteMemory::GetLoadName(chosen),
			matched >= 0 ? "content" : "time");
	}
}

void AssignFromObservation()
{
	for (int player = 0; player < 2; ++player)
	{
		const int index = PaletteTexture::FindByPointer(g_observed[player]);
		if (index < 0 || PaletteTexture::IsOwnerByHand(index))
			continue;

		PaletteTexture::TextureOwner owner = {};
		owner.kind = PaletteTexture::OwnerKind::Player;
		owner.players = PaletteTexture::PlayerBit(player);
		PaletteTexture::AssignOwner(index, owner, PaletteTexture::Claim::Strong);
	}

	const int first = PaletteTexture::FindByPointer(g_observed[0]);
	const int second = PaletteTexture::FindByPointer(g_observed[1]);
	if (first < 0 || second < 0)
		return;

	for (int i = 0; i < PaletteTexture::GetSeenCount(); ++i)
	{
		if (i == first || i == second)
			continue;

		if (PaletteTexture::IsOwnerByHand(i) || !PaletteTexture::WasBoundInMatch(i))
			continue;

		const PaletteTexture::TextureOwner current = PaletteTexture::GetOwnerInfo(i);
		if (current.kind != PaletteTexture::OwnerKind::Unknown &&
			current.kind != PaletteTexture::OwnerKind::Player)
		{
			continue;
		}

		PaletteTexture::TextureOwner effect = {};
		effect.kind = PaletteTexture::OwnerKind::Effect;
		PaletteTexture::AssignOwner(i, effect, PaletteTexture::Claim::Guess);
	}
}

}

bool PaletteBinder::IsAuthoritative()
{
	return g_modVals.paletteOwnersFromDraws && g_haveObservation;
}

int PaletteBinder::GetObservedTexture(int player)
{
	if (player < 0 || player > 1)
		return -1;

	return PaletteTexture::FindByPointer(g_observed[player]);
}

void PaletteBinder::OnFrame()
{
	const bool inMatch = GameState::IsInMatch();
	const int seenCount = PaletteTexture::GetSeenCount();

	const bool wantCapture = inMatch || g_modVals.paletteOutOfMatch;

	if (wantCapture != g_lastWantCapture)
	{
		g_lastWantCapture = wantCapture;
		PaletteDrawProbe::SetCapturing(wantCapture);
	}

	if (inMatch != g_lastInMatch || seenCount != g_lastSeenCount)
	{
		g_lastInMatch = inMatch;
		g_lastSeenCount = seenCount;

		if (wantCapture && !PaletteDrawProbe::IsCapturing())
			PaletteDrawProbe::SetCapturing(true);
	}

	const int generation = PaletteTexture::GetGeneration();
	if (generation != g_lastGeneration)
	{
		g_lastGeneration = generation;
		PaletteDrawProbe::ResetTuples();
		g_observed[0] = 0;
		g_observed[1] = 0;
		g_haveObservation = false;
		g_loggedBinder[0] = g_loggedBinder[1] = -1;
		g_loggedGuess[0] = g_loggedGuess[1] = -1;
		g_nameIn = 0;
		g_namedByDraws = false;
		return;
	}

	if (!inMatch)
	{
		g_haveObservation = false;

		if (g_modVals.paletteOutOfMatch && --g_attributeIn <= 0)
		{
			g_attributeIn = kAttributeEveryFrames;
			AttributeOutOfMatchTextures();
		}

		return;
	}

	LoadSignatures();

	uintptr_t byRow[2] = {};
	ObserveCharacterTextures(byRow);

	const bool rowsSwapped = PaletteTexture::GetRowsSwapped();
	g_observed[0] = byRow[rowsSwapped ? 1 : 0];
	g_observed[1] = byRow[rowsSwapped ? 0 : 1];

	g_haveObservation = g_observed[0] != 0 || g_observed[1] != 0;

	if (g_modVals.paletteOwnersFromDraws && --g_nameIn <= 0)
	{
		g_nameIn = kNameEveryFrames;
		NameSidesFromDraws();
	}

	CompareWithGuess();
}
