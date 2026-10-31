#include "Game/Menus/LobbyAvatar.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Network/LobbyPaletteCodec.h"
#include "Network/LobbyPalettes.h"
#include "Palette/NetworkPick.h"
#include "Palette/PaletteLibrary.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteSignature.h"
#include "Palette/SelectLayer.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int kSides = PalettePaint::kPlayers;
constexpr int kNoCharacter = -1;
constexpr int kNoColour = -1;
constexpr int kCandidates = LobbyPalettes::kMaxMembers + 1;
constexpr uint64_t kLocalId = 0;

struct LocalAvatar
{
	NetworkPick::Pick pick;
	char file[NetworkPick::kFileLength];
	bool loaded;
	bool shared;
	uint8_t colours[PalettePaint::kBytes];
	uint8_t effects[PalettePaint::kBytes];
};

struct Candidate
{
	uint64_t id;
	uint8_t signature[PaletteSignature::kBytes];
	uint8_t colours[PalettePaint::kBytes];
	uint8_t effects[PalettePaint::kBytes];
};

LocalAvatar g_local = { { kNoCharacter, kNoColour, "", false, {} }, "", false, false, {}, {} };
unsigned g_localRevision = 0;

Candidate g_candidates[kCandidates] = {};

struct Seat
{
	bool decided;
	unsigned revision;
	unsigned localRevision;
	uint8_t signature[PaletteSignature::kBytes];
	bool staged;
	char status[64];
};

Seat g_seats[kSides] = {};
char g_status[160] = "not in a room";

bool IsUsable(const LocalAvatar& avatar)
{
	return avatar.loaded && avatar.pick.hasSignature;
}

void Publish()
{
	char text[LobbyPaletteCodec::kTextBytes] = {};
	char effects[LobbyPaletteCodec::kEffectTextBytes] = {};
	const bool share = g_local.shared && IsUsable(g_local) &&
		LobbyPaletteCodec::Encode(g_local.pick.chara, g_local.pick.colour, g_local.pick.signature, g_local.colours, text,
			sizeof(text)) &&
		LobbyPaletteCodec::EncodeEffects(g_local.effects, effects, sizeof(effects));

	LobbyPalettes::SetLocal(share ? text : "", share ? effects : "");
}

bool SamePick(const NetworkPick::Pick& pick, const std::string& file, bool shared)
{
	return pick.chara == g_local.pick.chara && pick.colour == g_local.pick.colour && file == g_local.file &&
		shared == g_local.shared && pick.hasSignature == g_local.pick.hasSignature &&
		memcmp(pick.signature, g_local.pick.signature, PaletteSignature::kBytes) == 0;
}

void RefreshLocal()
{
	NetworkPick::Pick pick = { kNoCharacter, kNoColour, "", false, {} };
	NetworkPick::Recall(pick);

	const std::string file = pick.chara == kNoCharacter ? std::string() : NetworkPick::FileFor(pick.chara, pick.colour);
	const bool shared = g_modVals.sharePalettes;

	if (SamePick(pick, file, shared))
		return;

	g_local.pick = pick;
	g_local.shared = shared;
	strncpy_s(g_local.file, file.c_str(), _TRUNCATE);
	g_local.loaded = PaletteLibrary::LoadWithEffects(pick.chara, g_local.file, g_local.colours, g_local.effects);
	++g_localRevision;

	if (g_local.loaded && !pick.hasSignature)
		LOG("lobby avatar: the network pick has no colour signature yet, confirm it again at Character Select");

	Publish();
}

int GatherLocal()
{
	if (!IsUsable(g_local))
		return 0;

	Candidate& local = g_candidates[0];
	local.id = kLocalId;
	memcpy(local.signature, g_local.pick.signature, PaletteSignature::kBytes);
	memcpy(local.colours, g_local.colours, PalettePaint::kBytes);
	memcpy(local.effects, g_local.effects, PalettePaint::kBytes);
	return 1;
}

int GatherMembers(int count)
{
	if (!g_modVals.showOnlinePalettes)
		return count;

	LobbyPalettes::Member member = {};

	for (int i = 0; i < LobbyPalettes::Count() && count < kCandidates; ++i)
	{
		if (!LobbyPalettes::At(i, member))
			continue;

		Candidate& candidate = g_candidates[count];
		candidate.id = member.id;
		memcpy(candidate.signature, member.entry.signature, PaletteSignature::kBytes);
		memcpy(candidate.colours, member.entry.rgba, PalettePaint::kBytes);
		memcpy(candidate.effects, member.effects, PalettePaint::kBytes);
		++count;
	}

	return count;
}

void Describe()
{
	sprintf_s(g_status, "p1 %s, p2 %s", g_seats[0].status[0] != '\0' ? g_seats[0].status : "not drawn",
		g_seats[1].status[0] != '\0' ? g_seats[1].status : "not drawn");
}

void WearNothing(int side)
{
	Seat& seat = g_seats[side];
	strncpy_s(seat.status, "the game's colours", _TRUNCATE);

	if (!seat.staged)
		return;

	seat.staged = false;
	SelectLayer::Clear(side);
	LOG("lobby avatar: p%d wears the game's colours", side + 1);
}

void Wear(int side, const Candidate& candidate)
{
	Seat& seat = g_seats[side];

	if (candidate.id == kLocalId)
		strncpy_s(seat.status, "our palette", _TRUNCATE);
	else
		sprintf_s(seat.status, "member %llu", static_cast<unsigned long long>(candidate.id));

	seat.staged = true;
	SelectLayer::Stage(side, candidate.colours, candidate.effects);
	LOG("lobby avatar: p%d wears %s", side + 1, seat.status);
}

void Decide(int side, const uint8_t* shown, int count)
{
	const uint8_t* signatures[kCandidates] = {};

	for (int i = 0; i < count; ++i)
		signatures[i] = g_candidates[i].signature;

	const int match = PaletteSignature::FindMatch(shown, signatures, count);

	if (match == PaletteSignature::kNone)
	{
		WearNothing(side);
		return;
	}

	Wear(side, g_candidates[match]);
}

bool AlreadyDecided(const Seat& seat, const uint8_t* signature, unsigned revision)
{
	return seat.decided && revision == seat.revision && g_localRevision == seat.localRevision &&
		memcmp(signature, seat.signature, PaletteSignature::kBytes) == 0;
}

void Follow(int side, unsigned revision, int& count)
{
	Seat& seat = g_seats[side];
	uint8_t shown[PalettePaint::kBytes] = {};

	if (!PalettePaint::ReadGameColours(side, shown))
		return;

	uint8_t signature[PaletteSignature::kBytes] = {};
	PaletteSignature::Take(shown, signature);

	if (AlreadyDecided(seat, signature, revision))
		return;

	seat.decided = true;
	seat.revision = revision;
	seat.localRevision = g_localRevision;
	memcpy(seat.signature, signature, PaletteSignature::kBytes);

	if (count < 0)
		count = GatherMembers(GatherLocal());

	Decide(side, shown, count);
	Describe();
}

}

void LobbyAvatar::OnFrame()
{
	RefreshLocal();

	const unsigned revision = LobbyPalettes::Revision();
	int gathered = -1;

	for (int side = 0; side < kSides; ++side)
		Follow(side, revision, gathered);
}

void LobbyAvatar::Forget()
{
	for (int side = 0; side < kSides; ++side)
	{
		Seat& seat = g_seats[side];
		seat.decided = false;
		seat.status[0] = '\0';

		if (!seat.staged)
			continue;

		seat.staged = false;
		SelectLayer::Clear(side);
	}

	strncpy_s(g_status, "not in a room", _TRUNCATE);
}

const char* LobbyAvatar::StatusText()
{
	return g_status;
}
