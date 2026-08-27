#include "Network/NetplayTick.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Game/OpponentLog.h"
#include "Network/RollbackStats.h"
#include "Network/ModPresence.h"
#include "Network/PlayerCount.h"
#include "Network/RoomPing.h"
#include "Network/RoomRoster.h"
#include "Network/SteamInterfaces.h"
#include "Network/SteamNetwork.h"

namespace {

bool g_initialized = false;

}

bool NetplayTick::Initialize()
{
	if (g_initialized)
		return true;

	RoomRoster::Initialize();
	RoomRoster::SetFixEnabled(g_modVals.roomRosterFix);

	RoomPing::Initialize();
	RoomPing::SetEnabled(g_modVals.republishPingLocation);

	OpponentLog::Initialize();

	g_initialized = true;
	LOG("NetplayTick: room roster, ping refresh and opponent log are up");
	return true;
}

void NetplayTick::Update()
{
	if (!g_initialized)
		return;

	if (!SteamInterfaces::IsReady())
		SteamInterfaces::Initialize();

	if (!SteamNetwork::IsReady())
		SteamNetwork::Initialize();

	RollbackStats::Update();
	RoomPing::Update();
	ModPresence::Update();
	PlayerCount::Update();
	OpponentLog::Update();
}
