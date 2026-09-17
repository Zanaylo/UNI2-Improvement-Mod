#include "Network/ModChannel.h"

#include "Network/NetGate.h"
#include "Network/NetLog.h"
#include "Network/SteamNetwork.h"

#include <cstring>

namespace {

constexpr int kMaxRoutes = 8;
constexpr int kOutboxSlots = 16;
constexpr int kInboxSlots = 16;
constexpr int kSendsPerFlush = 4;
constexpr int kReadsPerReceive = 32;
constexpr int kLabelBytes = 24;

struct Route
{
	uint16_t kind;
	ModChannel::Handler handler;
};

struct Outgoing
{
	bool used;
	bool inFlight;
	bool holdLogged;
	bool refusedLogged;
	NetGate::Request request;
	char label[kLabelBytes];
	uint8_t data[ModChannel::kMaxBytes];
};

struct Incoming
{
	bool used;
	uint64_t from;
	int size;
	uint8_t data[ModChannel::kMaxBytes];
};

Route g_routes[kMaxRoutes] = {};
int g_routeCount = 0;

SRWLOCK g_outboxLock = SRWLOCK_INIT;
Outgoing g_outbox[kOutboxSlots] = {};

SRWLOCK g_inboxLock = SRWLOCK_INIT;
Incoming g_inbox[kInboxSlots] = {};

Outgoing g_sending = {};
Incoming g_arriving = {};
Incoming g_dispatching = {};

ModChannel::Handler Find(uint16_t kind)
{
	for (int i = 0; i < g_routeCount; ++i)
	{
		if (g_routes[i].kind == kind)
			return g_routes[i].handler;
	}

	return nullptr;
}

uint16_t KindOf(const uint8_t* data, int size)
{
	if (size < static_cast<int>(sizeof(ModChannel::Header)))
		return 0;

	ModChannel::Header header = {};
	memcpy(&header, data, sizeof(header));

	return header.magic == ModChannel::kMagic ? header.kind : 0;
}

bool Enqueue(bool toPeer, uint64_t to, const void* data, int size, DWORD ttlMs, const char* label)
{
	if (to == 0 || data == nullptr || size <= 0 || size > ModChannel::kMaxBytes)
		return false;

	AcquireSRWLockExclusive(&g_outboxLock);

	for (Outgoing& slot : g_outbox)
	{
		if (slot.used)
			continue;

		slot.used = true;
		slot.inFlight = false;
		slot.holdLogged = false;
		slot.refusedLogged = false;
		slot.request = { toPeer, to, GetTickCount(), ttlMs, size };
		strncpy_s(slot.label, label != nullptr ? label : "?", _TRUNCATE);
		memcpy(slot.data, data, static_cast<size_t>(size));

		ReleaseSRWLockExclusive(&g_outboxLock);
		return true;
	}

	ReleaseSRWLockExclusive(&g_outboxLock);
	NetLog::Write("mod send of %s dropped, the queue is full", label != nullptr ? label : "?");
	return false;
}

int Oldest()
{
	int oldest = -1;

	for (int i = 0; i < kOutboxSlots; ++i)
	{
		const Outgoing& slot = g_outbox[i];

		if (!slot.used || slot.inFlight)
			continue;

		if (oldest < 0 || slot.request.queuedAt < g_outbox[oldest].request.queuedAt)
			oldest = i;
	}

	return oldest;
}

int Checkout()
{
	AcquireSRWLockExclusive(&g_outboxLock);

	const int index = Oldest();

	if (index >= 0)
	{
		g_outbox[index].inFlight = true;
		g_sending = g_outbox[index];
	}

	ReleaseSRWLockExclusive(&g_outboxLock);
	return index;
}

void Settle(int index, bool release, bool keepInFlight, const Outgoing& state)
{
	AcquireSRWLockExclusive(&g_outboxLock);

	Outgoing& slot = g_outbox[index];
	slot.inFlight = keepInFlight;
	slot.used = !release;
	slot.holdLogged = state.holdLogged;
	slot.refusedLogged = state.refusedLogged;

	ReleaseSRWLockExclusive(&g_outboxLock);
}

bool Deliver(int index, const NetLink::Snapshot& snapshot, DWORD now)
{
	const char* why = "";
	const NetGate::Verdict verdict = NetGate::Judge(g_sending.request, snapshot, now, why);
	const unsigned long long to = static_cast<unsigned long long>(g_sending.request.to);

	if (verdict == NetGate::Verdict_Drop)
	{
		NetLog::Write("mod send of %s to %llu dropped: %s", g_sending.label, to, why);
		Settle(index, true, false, g_sending);
		return true;
	}

	if (verdict == NetGate::Verdict_Hold)
	{
		if (!g_sending.holdLogged && strcmp(why, "spacing") != 0)
			NetLog::Write("mod send of %s to %llu held: %s", g_sending.label, to, why);

		g_sending.holdLogged = true;
		Settle(index, false, true, g_sending);
		return false;
	}

	if (!SteamNetwork::SendTo(g_sending.request.to, g_sending.data, g_sending.request.size))
	{
		if (!g_sending.refusedLogged)
			NetLog::Write("mod send of %s to %llu refused by Steam, retrying until it expires", g_sending.label, to);

		g_sending.refusedLogged = true;
		Settle(index, false, true, g_sending);
		return false;
	}

	NetGate::NoteSent(g_sending.request, now);
	NetLog::Write("mod sent %s, %d B to %llu after %lu ms", g_sending.label, g_sending.request.size, to,
		now - g_sending.request.queuedAt);
	Settle(index, true, false, g_sending);
	return true;
}

bool Store()
{
	AcquireSRWLockExclusive(&g_inboxLock);

	for (Incoming& slot : g_inbox)
	{
		if (slot.used)
			continue;

		slot = g_arriving;
		slot.used = true;
		ReleaseSRWLockExclusive(&g_inboxLock);
		return true;
	}

	ReleaseSRWLockExclusive(&g_inboxLock);
	return false;
}

bool TakeIncoming()
{
	AcquireSRWLockExclusive(&g_inboxLock);

	for (Incoming& slot : g_inbox)
	{
		if (!slot.used)
			continue;

		g_dispatching = slot;
		slot.used = false;
		ReleaseSRWLockExclusive(&g_inboxLock);
		return true;
	}

	ReleaseSRWLockExclusive(&g_inboxLock);
	return false;
}

}

void ModChannel::Register(uint16_t kind, Handler handler)
{
	if (handler == nullptr)
		return;

	for (int i = 0; i < g_routeCount; ++i)
	{
		if (g_routes[i].kind != kind)
			continue;

		g_routes[i].handler = handler;
		return;
	}

	if (g_routeCount >= kMaxRoutes)
		return;

	g_routes[g_routeCount++] = { kind, handler };
}

bool ModChannel::SendToPeer(const void* data, int size, DWORD ttlMs, const char* label)
{
	return Enqueue(true, NetLink::Peer(), data, size, ttlMs, label);
}

bool ModChannel::SendTo(uint64_t to, const void* data, int size, DWORD ttlMs, const char* label)
{
	return Enqueue(false, to, data, size, ttlMs, label);
}

void ModChannel::Flush(const NetLink::Snapshot& snapshot)
{
	if (!SteamNetwork::IsReady())
		return;

	const DWORD now = GetTickCount();
	int attempts = 0;
	int held[kOutboxSlots] = {};
	int heldCount = 0;

	for (int index = Checkout(); index >= 0 && attempts < kSendsPerFlush; index = Checkout(), ++attempts)
	{
		if (!Deliver(index, snapshot, now))
			held[heldCount++] = index;
	}

	if (heldCount == 0)
		return;

	AcquireSRWLockExclusive(&g_outboxLock);

	for (int i = 0; i < heldCount; ++i)
		g_outbox[held[i]].inFlight = false;

	ReleaseSRWLockExclusive(&g_outboxLock);
}

void ModChannel::Receive()
{
	if (!SteamNetwork::IsReady())
		return;

	for (int i = 0; i < kReadsPerReceive; ++i)
	{
		int size = 0;
		uint64_t from = 0;

		if (!SteamNetwork::Receive(g_arriving.data, sizeof(g_arriving.data), size, from))
			return;

		const uint16_t kind = KindOf(g_arriving.data, size);

		if (kind == 0)
		{
			NetLog::Write("mod channel: %d B from %llu without the mod's header, ignored", size,
				static_cast<unsigned long long>(from));
			continue;
		}

		g_arriving.from = from;
		g_arriving.size = size;

		NetLog::Write("mod received kind %u, %d B from %llu", kind, size, static_cast<unsigned long long>(from));

		if (!Store())
			NetLog::Write("mod receive from %llu dropped, the inbox is full", static_cast<unsigned long long>(from));
	}
}

void ModChannel::Pump()
{
	for (int i = 0; i < kInboxSlots && TakeIncoming(); ++i)
	{
		const Handler handler = Find(KindOf(g_dispatching.data, g_dispatching.size));

		if (handler != nullptr)
			handler(g_dispatching.data, g_dispatching.size, g_dispatching.from);
	}
}

int ModChannel::Queued()
{
	int queued = 0;

	AcquireSRWLockShared(&g_outboxLock);

	for (const Outgoing& slot : g_outbox)
		queued += slot.used ? 1 : 0;

	ReleaseSRWLockShared(&g_outboxLock);

	return queued;
}
