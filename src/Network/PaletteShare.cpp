#include "Network/PaletteShare.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Network/ModChannel.h"
#include "Network/ModHandshake.h"
#include "Network/ModPresence.h"
#include "Network/NetLink.h"
#include "Network/Steam/SteamNetwork.h"
#include "Palette/EffectPaint.h"
#include "Palette/PaletteChoice.h"
#include "Palette/PaletteControl.h"
#include "Palette/PaletteFile.h"
#include "Palette/PaletteMemory.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteTrace.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kMagic = ModChannel::kMagic;
constexpr uint16_t kVersionOne = 1;
constexpr uint16_t kVersionTwo = 2;
constexpr uint16_t kVersionThree = 3;
constexpr uint16_t kKindPalette = ModChannel::kKindPalette;

#pragma pack(push, 1)
struct PacketV1
{
	uint32_t magic;
	uint16_t version;
	uint16_t kind;
	int32_t chara;
	char name[PaletteFile::kNameLength];
	uint8_t colors[PaletteFile::kBytes];
};

struct PacketV2
{
	uint32_t magic;
	uint16_t version;
	uint16_t kind;
	int32_t chara;
	int8_t side;
	char name[PaletteFile::kNameLength];
	uint8_t colors[PaletteFile::kBytes];
};

struct PacketV3
{
	uint32_t magic;
	uint16_t version;
	uint16_t kind;
	int32_t chara;
	int8_t side;
	char name[PaletteFile::kNameLength];
	uint8_t colors[PaletteFile::kBytes];
	uint8_t hasEffect;
	uint8_t effectColors[PaletteFile::kBytes];
};
#pragma pack(pop)

constexpr int kSendDelayFrames = 120;

constexpr int kResendFrames = 180;
constexpr int kResendsKnown = 3;

constexpr int kSettleFrames = 30;

constexpr DWORD kSendTtlMs = 30000;

bool g_inMatch = false;
int g_frames = 0;
int g_sent = 0;
int g_resends = 0;
uint64_t g_matchPeer = 0;
bool g_sideLogged = false;

unsigned g_seenRevision = 0;
int g_settled = 0;
unsigned g_sentRevision = 0;
bool g_everSent = false;

int g_sentCount = 0;
int g_receivedCount = 0;
char g_status[192] = "nothing sent or received yet";

struct Remote
{
	bool valid;
	int chara;
	char name[PaletteFile::kNameLength];
};

Remote g_remote[2] = {};

int g_statusStamp = -1;

int ReadOwnSide();

void UpdateStatus()
{
	const int seat = ReadOwnSide();

	const int stamp = (g_sentCount << 6) | (g_receivedCount << 4) | ((seat + 1) << 2) |
		(SteamNetwork::IsReady() ? 2 : 0) | (NetLink::HasPeer() ? 1 : 0);

	if (stamp == g_statusStamp)
		return;

	g_statusStamp = stamp;

	char seatText[32] = {};

	if (seat < 0)
		strncpy_s(seatText, "seat unknown, both are yours", _TRUNCATE);
	else
		sprintf_s(seatText, "you are p%d", seat + 1);

	sprintf_s(g_status, "%s, %s, sent %d, received %d%s",
		SteamNetwork::IsReady() ? "Steam ready" : "no Steam", seatText,
		g_sentCount, g_receivedCount,
		NetLink::HasPeer() ? "" : ", no peer yet");
}

int Resends()
{
	const bool known = ModHandshake::PeerHasMod() ||
		ModPresence::PeerHasMod(NetLink::Peer());

	return known ? kResendsKnown : 0;
}

int ReadOwnSide()
{
	uint32_t side = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kPlayerSideIndex)),
		side))
	{
		return -1;
	}

	return side <= 1 ? static_cast<int>(side) : -1;
}

int OwnPlayer()
{
	if (PaletteControl::IsSpectating())
		return -1;

	const int local = PaletteControl::LocalPlayer();

	return local >= 0 ? local : ReadOwnSide();
}

unsigned OwnRevision(int player)
{
	if (player < 0)
		return 0;

	return PalettePaint::GetRevision(player) * 131u + EffectPaint::GetRevision(player);
}

int SideForChara(int chara)
{
	int found = -1;

	for (int player = 0; player < 2; ++player)
	{
		if (PaletteMemory::GetCharaNumber(player) != chara)
			continue;

		if (found >= 0)
			return -1;

		found = player;
	}

	return found;
}

int SideForPeer(int chara, int claimed)
{
	if (!PaletteControl::IsSpectating())
	{
		const int own = OwnPlayer();

		if (own == 0 || own == 1)
			return 1 - own;
	}

	const int byChara = SideForChara(chara);

	if (byChara >= 0)
		return byChara;

	return claimed == 0 || claimed == 1 ? claimed : -1;
}

void WornName(int player, char* out, int size)
{
	strncpy_s(out, size, PaletteChoice::WornFile(player), _TRUNCATE);

	const size_t length = strlen(out);
	const size_t suffix = strlen(PaletteFile::kExtension);

	if (length > suffix && _stricmp(out + length - suffix, PaletteFile::kExtension) == 0)
		out[length - suffix] = 0;
}

void SendOurs()
{
	if (!g_modVals.sharePalettes)
	{
		PaletteTrace::Note("send skipped, SharePalettes is off");
		return;
	}

	const int own = OwnPlayer();

	if (own < 0)
	{
		PaletteTrace::Note("send skipped, our seat is unknown");
		return;
	}

	const uint8_t* colors = PalettePaint::GetStaged(own);

	uint8_t original[PaletteFile::kBytes] = {};

	char name[PaletteFile::kNameLength] = {};

	const bool bare = colors == nullptr;

	if (bare)
	{

		if (!g_everSent)
		{
			PaletteTrace::Note("send skipped, p%d has never worn one of ours", own);
			return;
		}

		if (!PalettePaint::ReadGameColours(own, original))
		{
			PaletteTrace::Note("send skipped, p%d has no palette and no original to fall back on",
				own);
			return;
		}

		colors = original;
	}
	else
	{
		WornName(own, name, sizeof(name));
	}

	uint8_t effects[EffectPaint::kBlockBytes] = {};
	const bool hasEffects = !bare && EffectPaint::GetEditedCount(own) > 0;

	if (hasEffects)
		EffectPaint::GetBlock(own, effects);

	PacketV3 packet = {};
	packet.magic = kMagic;
	packet.version = kVersionThree;
	packet.kind = kKindPalette;
	packet.chara = PaletteMemory::GetCharaNumber(own);
	packet.side = static_cast<int8_t>(own);
	strncpy_s(packet.name, name, _TRUNCATE);
	memcpy(packet.colors, colors, sizeof(packet.colors));

	packet.hasEffect = hasEffects ? 1 : 0;

	if (hasEffects)
		memcpy(packet.effectColors, effects, sizeof(packet.effectColors));

	if (!ModChannel::SendToPeer(&packet, sizeof(packet), kSendTtlMs, "palette"))
	{
		PaletteTrace::Note("send of '%s' was not queued", packet.name);
		return;
	}

	g_sentRevision = OwnRevision(own);
	g_everSent = true;
	++g_sentCount;

	PaletteTrace::Note("sent '%s' chara %d from seat %d with %s, send %d of %d", packet.name,
		packet.chara, own, packet.hasEffect ? "effects" : "no effects", g_sent, Resends());

	LOG("PaletteShare: queued '%s' for character %d on side %d", packet.name, packet.chara, own);
}

void PlaceForeign(int side, int chara, const char* name, const uint8_t* colors,
	const uint8_t* effectColors)
{
	if (side < 0 || side > 1 || colors == nullptr)
		return;

	PalettePaint::StageRemote(side, colors);
	EffectPaint::SetRemote(side, effectColors);

	Remote& remote = g_remote[side];
	remote.valid = true;
	remote.chara = chara;
	strncpy_s(remote.name, name != nullptr ? name : "", _TRUNCATE);

	++g_receivedCount;

	PaletteTrace::Note("p%d is wearing '%s' from the other player, effects %s", side, remote.name,
		effectColors != nullptr ? "included" : "none");
}

struct Incoming
{
	int chara;
	int claimed;
	const char* name;
	const uint8_t* colors;
	const uint8_t* effects;
};

bool ReadHeader(const uint8_t* data, int size, uint16_t& outVersion)
{
	if (size < static_cast<int>(sizeof(uint32_t) + sizeof(uint16_t) * 2))
		return false;

	uint32_t magic = 0;
	uint16_t kind = 0;
	memcpy(&magic, data, sizeof(magic));
	memcpy(&outVersion, data + 4, sizeof(outVersion));
	memcpy(&kind, data + 6, sizeof(kind));

	return magic == kMagic && kind == kKindPalette;
}

bool Unpack(const uint8_t* data, int size, uint16_t version, PacketV3& out, Incoming& incoming)
{
	if (version == kVersionThree && size == static_cast<int>(sizeof(PacketV3)))
	{
		memcpy(&out, data, sizeof(out));
	}
	else if (version == kVersionTwo && size == static_cast<int>(sizeof(PacketV2)))
	{
		PacketV2 packet = {};
		memcpy(&packet, data, sizeof(packet));

		out.chara = packet.chara;
		out.side = packet.side;
		memcpy(out.name, packet.name, sizeof(out.name));
		memcpy(out.colors, packet.colors, sizeof(out.colors));
		out.hasEffect = 0;
	}
	else if (version == kVersionOne && size == static_cast<int>(sizeof(PacketV1)))
	{
		PacketV1 packet = {};
		memcpy(&packet, data, sizeof(packet));

		out.chara = packet.chara;
		out.side = -1;
		memcpy(out.name, packet.name, sizeof(out.name));
		memcpy(out.colors, packet.colors, sizeof(out.colors));
		out.hasEffect = 0;
	}
	else
	{
		return false;
	}

	out.name[PaletteFile::kNameLength - 1] = 0;

	incoming.chara = out.chara;
	incoming.claimed = out.side;
	incoming.name = out.name;
	incoming.colors = out.colors;
	incoming.effects = out.hasEffect != 0 ? out.effectColors : nullptr;

	return true;
}

void HandlePacket(const uint8_t* data, int size, uint64_t from)
{
	uint16_t version = 0;

	if (!ReadHeader(data, size, version))
		return;

	if (from == 0 || from != NetLink::Peer())
	{
		PaletteTrace::Note("a packet from %llu is not the opponent's, dropped",
			(unsigned long long)from);
		return;
	}

	if (!g_modVals.showOnlinePalettes)
	{
		PaletteTrace::Note("a packet arrived but ShowOnlinePalettes is off");
		return;
	}

	PacketV3 packet = {};
	Incoming incoming = {};

	if (!Unpack(data, size, version, packet, incoming))
	{
		PaletteTrace::Note("a packet of %d bytes, version %u, was ignored", size, version);
		LOG("PaletteShare: a packet of %d bytes, version %u, was ignored", size, version);
		return;
	}

	const int own = OwnPlayer();
	const int side = SideForPeer(incoming.chara, incoming.claimed);

	PaletteTrace::Note("got '%s' chara %d with %s, marked seat %d, our seat %d, so it is p%d's",
		incoming.name, incoming.chara, incoming.effects != nullptr ? "effects" : "no effects",
		incoming.claimed, own, side);

	if (side < 0 || side == own)
		return;

	PlaceForeign(side, incoming.chara, incoming.name, incoming.colors, incoming.effects);
}

void DropStaleForeign()
{
	for (int side = 0; side < 2; ++side)
	{
		if (!g_remote[side].valid)
			continue;

		const int chara = PaletteMemory::GetCharaNumber(side);

		if (chara < 0 || chara == g_remote[side].chara)
			continue;

		PalettePaint::ClearRemote(side);
		EffectPaint::ClearRemote(side);

		PaletteTrace::Note("p%d changed character, so '%s' from the other player was dropped",
			side, g_remote[side].name);

		g_remote[side] = {};
	}
}

void ForgetForeign()
{
	for (int side = 0; side < 2; ++side)
	{
		PalettePaint::ClearRemote(side);
		EffectPaint::ClearRemote(side);

		g_remote[side] = {};
	}
}

}

void PaletteShare::Initialize()
{
	ModChannel::Register(ModChannel::kKindPalette, &HandlePacket);
	UpdateStatus();
}

int PaletteShare::GetOwnSide()
{
	return ReadOwnSide();
}

const char* PaletteShare::GetRemoteName(int player)
{
	if (player < 0 || player > 1 || !g_remote[player].valid)
		return "";

	return g_remote[player].name;
}

void PaletteShare::InjectTestPacket(int side)
{
	PacketV2 packet = {};
	packet.magic = kMagic;
	packet.version = kVersionTwo;
	packet.kind = kKindPalette;
	packet.chara = PaletteMemory::GetCharaNumber(side);
	packet.side = static_cast<int8_t>(side);
	strncpy_s(packet.name, "injected test", _TRUNCATE);

	for (int c = 0; c < 256; ++c)
	{
		packet.colors[c * 4 + 0] = static_cast<uint8_t>(c);
		packet.colors[c * 4 + 1] = static_cast<uint8_t>(255 - c);
		packet.colors[c * 4 + 2] = static_cast<uint8_t>(c * 5);
		packet.colors[c * 4 + 3] = 255;
	}

	LOG("PaletteShare: injecting a test packet for side %d", side);

	PlaceForeign(side, packet.chara, packet.name, packet.colors, nullptr);
}

void PaletteShare::OnFrame()
{
	if (!SteamNetwork::IsReady())
		return;

	const bool inMatch = GameState::IsInMatch();

	if (inMatch != g_inMatch)
	{
		g_inMatch = inMatch;
		g_frames = 0;
		g_sent = 0;
		g_resends = 0;
		g_sideLogged = false;
		g_everSent = false;
		g_sentRevision = 0;
		g_settled = 0;

		if (!inMatch)
		{
			ForgetForeign();
			g_matchPeer = 0;
		}
	}

	if (!inMatch)
		return;

	DropStaleForeign();

	++g_frames;

	if (!NetLink::HasPeer())
		return;

	if (g_matchPeer == 0)
		g_matchPeer = NetLink::Peer();

	if (!g_sideLogged)
	{
		g_sideLogged = true;
		LOG("PaletteShare: peer is up and [0x597940] reads %d", GetOwnSide());
	}

	const int own = OwnPlayer();

	if (own < 0)
	{
		UpdateStatus();
		return;
	}

	const unsigned revision = OwnRevision(own);

	if (revision != g_seenRevision)
	{
		g_seenRevision = revision;
		g_settled = 0;
	}
	else if (g_settled < kSettleFrames)
	{
		++g_settled;
	}

	if (g_everSent && g_settled == kSettleFrames && revision != g_sentRevision)
	{
		PaletteTrace::Note("p%d changed, sending it again", own);

		g_sent = 0;
		g_frames = kSendDelayFrames;
		++g_resends;
	}

	const int resends = Resends();

	if (g_sent < resends && g_frames >= kSendDelayFrames + g_sent * kResendFrames)
	{
		++g_sent;
		SendOurs();
	}

	UpdateStatus();
}

void PaletteShare::GetDiagnostics(Diagnostics& out)
{
	out = {};

	out.ownSide = OwnPlayer();
	out.framesInMatch = g_frames;
	out.sendsDone = g_sent;
	out.sendsAllowed = Resends();
	out.nextSendFrame = g_sent < out.sendsAllowed ? kSendDelayFrames + g_sent * kResendFrames : -1;
	out.sent = g_sentCount;
	out.received = g_receivedCount;
	out.matchPeer = g_matchPeer;
	out.steamAttempts = 0;
	out.lastSentChoice = static_cast<int>(g_sentRevision);
	out.resends = g_resends;

	for (int side = 0; side < 2; ++side)
	{
		out.pendingValid[side] = g_remote[side].valid;
		out.pendingApplied[side] = g_remote[side].valid;
		out.pendingChara[side] = g_remote[side].chara;
		out.pendingName[side] = g_remote[side].name;
	}
}

const char* PaletteShare::GetStatusText()
{
	return g_status;
}
