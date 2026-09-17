#include "Network/NetGate.h"

#include "Game/GameOffsets.h"

namespace {

NetGate::PeerVerifier g_verifier = nullptr;

uint64_t g_budgetPeer = 0;
volatile LONG g_peerBytes = 0;
DWORD g_lastPeerSend = 0;
DWORD g_lastDirectSend = 0;
DWORD g_directWindowStart = 0;
int g_directWindowBytes = 0;

void Rebudget(uint64_t peer)
{
	if (peer == g_budgetPeer)
		return;

	g_budgetPeer = peer;
	InterlockedExchange(&g_peerBytes, 0);
	g_lastPeerSend = 0;
}

NetGate::Verdict Hold(const char*& why, const char* reason)
{
	why = reason;
	return NetGate::Verdict_Hold;
}

NetGate::Verdict Drop(const char*& why, const char* reason)
{
	why = reason;
	return NetGate::Verdict_Drop;
}

bool LinkBusy(const NetLink::Snapshot& snapshot)
{
	return snapshot.hasPeer && snapshot.peer.pending > NetGate::kBusyPendingFrames;
}

NetGate::Verdict JudgePeer(const NetGate::Request& request, const NetLink::Snapshot& snapshot, DWORD now,
	const char*& why)
{
	if (!snapshot.hasPeer)
		return Hold(why, "no match is connected");

	if (snapshot.peer.id != request.to)
		return Drop(why, "the opponent changed");

	Rebudget(snapshot.peer.id);

	if (g_verifier == nullptr || !g_verifier(request.to))
		return Drop(why, "the opponent is not known to run the mod, so nothing is sent to them");

	if (snapshot.synchronizing || snapshot.peer.state != GameOffsets::kGgpoStateRunning)
		return Hold(why, "the match connection is still starting");

	if (LinkBusy(snapshot))
		return Hold(why, "the match connection is busy");

	if (g_peerBytes + request.size > NetGate::kPeerBudgetBytes)
		return Drop(why, "the per match budget is spent");

	if (g_lastPeerSend != 0 && now - g_lastPeerSend < NetGate::kPeerSpacingMs)
		return Hold(why, "spacing");

	return NetGate::Verdict_Send;
}

NetGate::Verdict JudgeDirect(const NetGate::Request& request, const NetLink::Snapshot& snapshot, DWORD now,
	const char*& why)
{
	if (LinkBusy(snapshot))
		return Hold(why, "the match connection is busy");

	if (now - g_directWindowStart >= 1000)
	{
		g_directWindowStart = now;
		g_directWindowBytes = 0;
	}

	if (g_directWindowBytes + request.size > NetGate::kDirectBytesPerSecond)
		return Hold(why, "spacing");

	if (g_lastDirectSend != 0 && now - g_lastDirectSend < NetGate::kDirectSpacingMs)
		return Hold(why, "spacing");

	return NetGate::Verdict_Send;
}

}

void NetGate::SetPeerVerifier(PeerVerifier verifier)
{
	g_verifier = verifier;
}

NetGate::Verdict NetGate::Judge(const Request& request, const NetLink::Snapshot& snapshot, DWORD now, const char*& why)
{
	why = "";

	if (request.to == 0)
		return Drop(why, "no recipient");

	if (now - request.queuedAt > request.ttlMs)
		return Drop(why, "it waited too long");

	if (request.toPeer || (snapshot.hasPeer && request.to == snapshot.peer.id))
		return JudgePeer(request, snapshot, now, why);

	return JudgeDirect(request, snapshot, now, why);
}

void NetGate::NoteSent(const Request& request, DWORD now)
{
	if (request.toPeer || request.to == g_budgetPeer)
	{
		g_lastPeerSend = now;
		InterlockedExchangeAdd(&g_peerBytes, request.size);
		return;
	}

	g_lastDirectSend = now;
	g_directWindowBytes += request.size;
}

bool NetGate::MayTouchRoom(const NetLink::Snapshot& snapshot)
{
	return !NetLink::InSession(snapshot);
}

bool NetGate::MayTouchRoom()
{
	return !NetLink::InSession();
}

int NetGate::PeerBytesThisSession()
{
	return static_cast<int>(g_peerBytes);
}
