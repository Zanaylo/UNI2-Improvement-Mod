#include "Network/ModHandshake.h"

#include "Core/info.h"
#include "Core/logger.h"
#include "Game/GamePatches.h"
#include "Network/ModChannel.h"
#include "Network/SteamNetwork.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr uint16_t kHelloVersion = 1;
constexpr uint8_t kFlagAnswerMe = 1;

constexpr int kAskEveryFrames = 30;
constexpr int kAsks = 3;
constexpr int kVerdictFrames = 90;

constexpr int kNotSent = -2;
constexpr int kVersionBytes = 16;
constexpr int kIdBytes = ModHandshake::kDataIdBytes;

#pragma pack(push, 1)
struct Hello
{
	ModChannel::Header header;
	uint8_t flags;
	char modVersion[kVersionBytes];
	char wanted[kIdBytes];
	char loaded[kIdBytes];
};
#pragma pack(pop)

uint64_t g_peer = 0;
int g_waited = 0;
int g_sentChosen = kNotSent;
int g_sentBoot = kNotSent;

ModHandshake::PeerState g_state = ModHandshake::Peer_None;
char g_peerVersion[kVersionBytes] = "";
char g_peerWanted[kIdBytes] = "";
char g_peerLoaded[kIdBytes] = "";

char g_status[192] = "no session";

void UpdateStatus()
{
	switch (g_state)
	{
	case ModHandshake::Peer_Waiting:
		strncpy_s(g_status, "connected, waiting for the other side's hello", _TRUNCATE);
		return;
	case ModHandshake::Peer_Modded:
		sprintf_s(g_status, "the other side runs %s, picked %s, has %s loaded", g_peerVersion,
			g_peerWanted, g_peerLoaded);
		return;
	case ModHandshake::Peer_Unmodded:
		strncpy_s(g_status, "the other side sent no hello, so it has no mod", _TRUNCATE);
		return;
	default:
		strncpy_s(g_status, "no session", _TRUNCATE);
		return;
	}
}

bool LocalChanged()
{
	return GamePatches::HomeIndex() != g_sentChosen || GamePatches::BootIndex() != g_sentBoot;
}

bool SendHello(uint8_t flags)
{
	Hello hello = {};
	hello.header.magic = ModChannel::kMagic;
	hello.header.version = kHelloVersion;
	hello.header.kind = ModChannel::kKindHello;
	hello.flags = flags;
	strncpy_s(hello.modVersion, UNI2_IM_VERSION, _TRUNCATE);

	const int chosen = GamePatches::HomeIndex();
	const int boot = GamePatches::BootIndex();

	ModHandshake::DescribeData(chosen, hello.wanted, sizeof(hello.wanted));
	ModHandshake::DescribeData(boot, hello.loaded, sizeof(hello.loaded));

	if (!SteamNetwork::Send(&hello, sizeof(hello)))
		return false;

	g_sentChosen = chosen;
	g_sentBoot = boot;

	LOG("ModHandshake: hello to %llu, picked %s, %s loaded%s", static_cast<unsigned long long>(g_peer),
		hello.wanted, hello.loaded, (flags & kFlagAnswerMe) != 0 ? ", asking for an answer" : "");
	return true;
}

void Track(uint64_t peer)
{
	if (peer == g_peer)
		return;

	g_peer = peer;
	g_waited = 0;
	g_sentChosen = kNotSent;
	g_sentBoot = kNotSent;
	g_peerVersion[0] = 0;
	g_peerWanted[0] = 0;
	g_peerLoaded[0] = 0;
	g_state = peer != 0 ? ModHandshake::Peer_Waiting : ModHandshake::Peer_None;

	UpdateStatus();

	if (peer != 0)
		LOG("ModHandshake: session with %llu", static_cast<unsigned long long>(peer));
}

void Wait()
{
	if (g_waited % kAskEveryFrames == 0 && g_waited < kAskEveryFrames * kAsks)
		SendHello(kFlagAnswerMe);

	if (++g_waited < kVerdictFrames)
		return;

	g_state = ModHandshake::Peer_Unmodded;
	UpdateStatus();

	LOG("ModHandshake: no hello from %llu in %d frames, it has no mod",
		static_cast<unsigned long long>(g_peer), kVerdictFrames);
}

void HandleHello(const uint8_t* data, int size, uint64_t from)
{
	if (size < static_cast<int>(sizeof(Hello)) || from == 0 || from != SteamNetwork::GetPeer())
		return;

	Track(from);

	Hello hello = {};
	memcpy(&hello, data, sizeof(hello));
	hello.modVersion[kVersionBytes - 1] = 0;
	hello.wanted[kIdBytes - 1] = 0;
	hello.loaded[kIdBytes - 1] = 0;

	g_state = ModHandshake::Peer_Modded;
	strncpy_s(g_peerVersion, hello.modVersion, _TRUNCATE);
	strncpy_s(g_peerWanted, hello.wanted, _TRUNCATE);
	strncpy_s(g_peerLoaded, hello.loaded, _TRUNCATE);
	UpdateStatus();

	LOG("ModHandshake: %llu runs %s, picked %s, %s loaded", static_cast<unsigned long long>(from),
		g_peerVersion, g_peerWanted, g_peerLoaded);

	if ((hello.flags & kFlagAnswerMe) == 0 || g_sentChosen != kNotSent)
		return;

	SendHello(0);
}

}

void ModHandshake::Initialize()
{
	ModChannel::Register(ModChannel::kKindHello, &HandleHello);
}

void ModHandshake::Update()
{
	Track(SteamNetwork::HasPeer() ? SteamNetwork::GetPeer() : 0);

	if (g_state == Peer_Waiting)
	{
		Wait();
		return;
	}

	if (g_state == Peer_Modded && LocalChanged())
		SendHello(0);
}

ModHandshake::PeerState ModHandshake::GetPeerState()
{
	return g_state;
}

bool ModHandshake::PeerHasMod()
{
	return g_state == Peer_Modded;
}

const char* ModHandshake::PeerVersion()
{
	return g_peerVersion;
}

const char* ModHandshake::PeerWanted()
{
	return g_peerWanted;
}

const char* ModHandshake::PeerLoaded()
{
	return g_peerLoaded;
}

void ModHandshake::DescribeData(int patchIndex, char* out, int size)
{
	const GamePatches::Patch* const patch = patchIndex >= 0 ? GamePatches::Get(patchIndex) : nullptr;

	if (patch == nullptr)
	{
		strncpy_s(out, size, kInstalled, _TRUNCATE);
		return;
	}

	if (patch->version > 0)
	{
		sprintf_s(out, size, "v%d", patch->version);
		return;
	}

	strncpy_s(out, size, patch->id.c_str(), _TRUNCATE);
}

int ModHandshake::IndexOfData(const char* id)
{
	if (id == nullptr || strcmp(id, kInstalled) == 0)
		return -1;

	char described[kDataIdBytes] = {};

	for (int i = 0; i < GamePatches::Count(); ++i)
	{
		DescribeData(i, described, sizeof(described));

		if (strcmp(described, id) == 0)
			return i;
	}

	return kNoData;
}

const char* ModHandshake::GetStatusText()
{
	return g_status;
}
