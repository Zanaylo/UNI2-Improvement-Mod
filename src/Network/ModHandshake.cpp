#include "Network/ModHandshake.h"

#include "Core/info.h"
#include "Game/GamePatches.h"
#include "Network/ModChannel.h"
#include "Network/ModPresence.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr uint16_t kHelloVersion = 1;
constexpr uint8_t kFlagAnswerMe = 1;

constexpr DWORD kHelloTtlMs = 20000;
constexpr DWORD kAnswerWaitMs = 30000;

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
volatile LONG64 g_heardFrom = 0;
DWORD g_trackedAt = 0;
bool g_asked = false;
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
		sprintf_s(g_status, "the other side runs %s, picked %s, has %s loaded", g_peerVersion, g_peerWanted,
			g_peerLoaded);
		return;
	case ModHandshake::Peer_Unmodded:
		strncpy_s(g_status, "the other side shows no sign of the mod, so nothing is sent to it", _TRUNCATE);
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

void QueueHello(uint8_t flags)
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

	if (!ModChannel::SendToPeer(&hello, sizeof(hello), kHelloTtlMs, (flags & kFlagAnswerMe) != 0 ? "hello" : "hello answer"))
		return;

	g_sentChosen = chosen;
	g_sentBoot = boot;
}

void Track(uint64_t peer)
{
	if (peer == g_peer)
		return;

	g_peer = peer;
	g_trackedAt = GetTickCount();
	g_asked = false;
	g_sentChosen = kNotSent;
	g_sentBoot = kNotSent;
	g_peerVersion[0] = 0;
	g_peerWanted[0] = 0;
	g_peerLoaded[0] = 0;
	g_state = peer != 0 ? ModHandshake::Peer_Waiting : ModHandshake::Peer_None;

	UpdateStatus();
}

void Wait()
{
	if (!ModPresence::PeerHasMod(g_peer))
	{
		g_state = ModHandshake::Peer_Unmodded;
		UpdateStatus();
		NetLog::Write("handshake: %llu has no mod marker in the room, the mod stays silent towards it",
			static_cast<unsigned long long>(g_peer));
		return;
	}

	if (!g_asked)
	{
		g_asked = true;
		QueueHello(kFlagAnswerMe);
		NetLog::Write("handshake: %llu carries the mod marker, hello queued", static_cast<unsigned long long>(g_peer));
	}

	if (GetTickCount() - g_trackedAt < kAnswerWaitMs)
		return;

	g_state = ModHandshake::Peer_Unmodded;
	UpdateStatus();
	NetLog::Write("handshake: no hello back from %llu", static_cast<unsigned long long>(g_peer));
}

void HandleHello(const uint8_t* data, int size, uint64_t from)
{
	if (size < static_cast<int>(sizeof(Hello)) || from == 0 || from != NetLink::Peer())
		return;

	Track(from);

	Hello hello = {};
	memcpy(&hello, data, sizeof(hello));
	hello.modVersion[kVersionBytes - 1] = 0;
	hello.wanted[kIdBytes - 1] = 0;
	hello.loaded[kIdBytes - 1] = 0;

	InterlockedExchange64(&g_heardFrom, static_cast<LONG64>(from));
	g_state = ModHandshake::Peer_Modded;
	strncpy_s(g_peerVersion, hello.modVersion, _TRUNCATE);
	strncpy_s(g_peerWanted, hello.wanted, _TRUNCATE);
	strncpy_s(g_peerLoaded, hello.loaded, _TRUNCATE);
	UpdateStatus();

	NetLog::Write("handshake: %llu runs %s, picked %s, %s loaded", static_cast<unsigned long long>(from), g_peerVersion,
		g_peerWanted, g_peerLoaded);

	if ((hello.flags & kFlagAnswerMe) == 0 || g_sentChosen != kNotSent)
		return;

	QueueHello(0);
}

}

void ModHandshake::Initialize()
{
	ModChannel::Register(ModChannel::kKindHello, &HandleHello);
}

void ModHandshake::Update()
{
	Track(NetLink::HasPeer() ? NetLink::Peer() : 0);

	if (g_state == Peer_Unmodded && !g_asked && ModPresence::PeerHasMod(g_peer))
		g_state = Peer_Waiting;

	if (g_state == Peer_Waiting)
	{
		Wait();
		return;
	}

	if (g_state == Peer_Modded && g_sentChosen != kNotSent && LocalChanged())
		QueueHello(0);
}

ModHandshake::PeerState ModHandshake::GetPeerState()
{
	return g_state;
}

bool ModHandshake::PeerHasMod()
{
	return g_state == Peer_Modded;
}

bool ModHandshake::HeardFrom(uint64_t id)
{
	return id != 0 && static_cast<uint64_t>(g_heardFrom) == id;
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
