#include "Network/NetplayTick.h"

#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GamePatches.h"
#include "Game/OnlineState.h"
#include "Game/OpponentLog.h"
#include "Network/GgpoLogCapture.h"
#include "Network/ModHandshake.h"
#include "Network/ModPresence.h"
#include "Network/NetGate.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/NetWorker.h"
#include "Network/OnlinePatch.h"
#include "Network/RollbackStats.h"
#include "Network/RoomPing.h"
#include "Network/RoomRoster.h"
#include "Network/Spectate.h"
#include "Network/SteamInterfaces.h"
#include "Network/SteamWatch.h"

namespace {

constexpr int kPickEveryFrames = 30;

bool g_initialized = false;
int g_frame = 0;

bool PeerVerified(uint64_t id)
{
	return ModHandshake::HeardFrom(id) || ModPresence::PeerHasMod(id);
}

void WriteHeader()
{
	NetLog::Write("mod %s, game build stamp 0x%08x, supported 0x%08x, addresses %s", UNI2_IM_VERSION, GetGameBuildStamp(),
		UNI2_IM_SUPPORTED_GAME_STAMP, IsMeasuredGameBuild() ? "resolved" : "NOT resolved, online features are off");

	NetLog::Write("settings: SharePalettes %d, ShowOnlinePalettes %d, RoomRosterFix %d, RepublishPingLocation %d, "
		"AllowSpectators %d, CaptureGgpoLog %d", g_settings.sharePalettes, g_settings.showOnlinePalettes,
		g_settings.roomRosterFix, g_settings.republishPingLocation, g_settings.spectateAllow, g_settings.netLogGgpo);

	NetLog::Write("frame pacing settings: TimerResolution %d, PowerThrottlingOptOut %d, PumpWait %d, PumpWaitAllInput %d, "
		"Supersample %d, PotatoMode %d", g_settings.timerResolution, g_settings.powerThrottlingOptOut, g_settings.pumpWait,
		g_settings.pumpWaitAllInput, g_settings.supersample, g_settings.potatoMode);
}

void PublishPick()
{
	if (++g_frame % kPickEveryFrames != 0)
		return;

	char pick[ModHandshake::kDataIdBytes] = {};
	ModHandshake::DescribeData(GamePatches::HomeIndex(), pick, sizeof(pick));
	ModPresence::SetPick(pick);
}

}

bool NetplayTick::Initialize()
{
	if (g_initialized)
		return true;

	NetLog::SetEnabled(g_modVals.netLog);
	NetLog::Initialize();
	WriteHeader();

	NetGate::SetPeerVerifier(&PeerVerified);

	RoomRoster::Initialize();
	RoomRoster::SetFixEnabled(g_modVals.roomRosterFix);
	RoomPing::SetEnabled(g_modVals.republishPingLocation);

	OpponentLog::Initialize();
	ModHandshake::Initialize();
	Spectate::Initialize();
	GgpoLogCapture::SetEnabled(g_modVals.netLogGgpo);

	PublishPick();
	NetWorker::Start();

	g_initialized = true;
	LOG("NetplayTick: network layer up, Steam work on its own thread, network log %s",
		g_modVals.netLog ? "on" : "off");
	return true;
}

void NetplayTick::Update()
{
	NetLink::Update();
	OnlineState::Update();

	if (!g_initialized)
		return;

	if (!SteamWatch::IsRegistered() && NetLink::Current().presentSeen && SteamInterfaces::Initialize())
		SteamWatch::Register();

	PublishPick();
	ModHandshake::Update();
	Spectate::Update();
	RollbackStats::Update();
	OnlinePatch::Update();
	OpponentLog::Update();
}

void NetplayTick::Shutdown()
{
	NetWorker::Stop();
	NetLog::Shutdown();
}
