#include "Overlay/Windows/NetplayWindow.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Game/Tables/CharaTables.h"
#include "Game/Lobby/IrHider.h"
#include "Game/Lobby/NameCensor.h"
#include "Game/Lobby/RoomNameCensor.h"
#include "Game/Lobby/OpponentLog.h"
#include "Network/RollbackStats.h"
#include "Game/Lobby/SteamNames.h"
#include "Network/BackgroundUpload.h"
#include "Network/GgpoLogCapture.h"
#include "Network/InputDelay.h"
#include "Network/InputDelayRule.h"
#include "Network/ModChannel.h"
#include "Network/ModHandshake.h"
#include "Network/ModPresence.h"
#include "Network/NetGate.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/NetWorker.h"
#include "Network/RoomPing.h"
#include "Network/RoomRoster.h"
#include "Network/RoundTripSmoothing.h"
#include "Network/TimeSyncTuning.h"
#include "Network/Steam/SteamLink.h"
#include "Network/Steam/SteamWatch.h"
#include "Network/Spectate/SpectateHost.h"
#include "Network/Spectate/SpectateViewer.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace {

constexpr float kDefaultWidth = 720.0f;
constexpr float kDefaultHeight = 560.0f;
constexpr float kPlotHeight = 70.0f;
constexpr float kSliderWidth = 240.0f;
constexpr const char* kNetplaySection = "Netplay";

const char* CharacterName(int chara)
{
	if (chara < 0)
		return "-";

	const char* name = CharaTables::Name(chara);
	return name != nullptr ? name : "?";
}

void Metric(const char* label, const char* format, ...)
{
	char value[128] = {};

	va_list args;
	va_start(args, format);
	vsnprintf(value, sizeof(value), format, args);
	va_end(args);

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(label);
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(value);
}

void PlotSeries(const char* label, const std::vector<float>& values, float maximum,
	const char* overlay)
{
	if (values.empty())
		return;

	ImGui::PlotLines(label, values.data(), static_cast<int>(values.size()), 0, overlay, 0.0f,
		maximum, ImVec2(0.0f, Ui::Scaled(kPlotHeight)));
}

float Maximum(const std::vector<float>& values, float floorValue)
{
	float highest = floorValue;

	for (float value : values)
	{
		if (value > highest)
			highest = value;
	}

	return highest;
}

NetcodeChoice::Values RunningNetcode()
{
	return { g_modVals.timeSyncInterval, g_modVals.timeSyncTail, g_modVals.timeSyncHalveGap,
		g_modVals.smoothRoundTrip };
}

void SaveNetcode(const NetcodeChoice::Values& values)
{
	Settings::SaveInt(kNetplaySection, "TimeSyncInterval", values.timeSyncInterval);
	Settings::SaveInt(kNetplaySection, "TimeSyncTail", values.timeSyncTail ? 1 : 0);
	Settings::SaveInt(kNetplaySection, "TimeSyncHalveGap", values.timeSyncHalveGap ? 1 : 0);
	Settings::SaveInt(kNetplaySection, "SmoothRoundTrip", values.smoothRoundTrip ? 1 : 0);
}

}

NetplayWindow::NetplayWindow(const std::string& title, bool closable, ImGuiWindowFlags windowFlags)
	: IWindow(title, closable, windowFlags)
{
}

void NetplayWindow::BeforeDraw()
{
	const ImGuiViewport* const viewport = ImGui::GetMainViewport();

	const float width = Ui::Scaled(kDefaultWidth);
	const float height = Ui::Scaled(kDefaultHeight);

	ImGui::SetNextWindowSize(ImVec2(width < viewport->WorkSize.x ? width : viewport->WorkSize.x,
		height < viewport->WorkSize.y ? height : viewport->WorkSize.y), ImGuiCond_FirstUseEver);

	ImGui::SetNextWindowSizeConstraints(Ui::Scaled(420.0f, 260.0f), viewport->WorkSize);
}

void NetplayWindow::Draw()
{
	if (!ImGui::BeginTabBar("##netplaytabs"))
		return;

	if (ImGui::BeginTabItem("Privacy"))
	{
		DrawPrivacyTab();
		ImGui::Separator();
		DrawRoomNamePrivacy();
		ImGui::Separator();
		DrawIrPrivacy();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Rollback"))
	{
		DrawRollbackTab();
		ImGui::Separator();
		DrawReplayUpload();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Netcode"))
	{
		DrawNetcodeTab();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Network log"))
	{
		DrawNetworkLogTab();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Opponents"))
	{
		DrawOpponentsTab();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
}

void NetplayWindow::DrawRollbackTab()
{
	if (!RollbackStats::IsNetplayActive())
	{
		UiText::Muted("This tab shows how smooth your online match is: ping, rollbacks and how far "
			"each side is behind. It only fills in during an online match.");
		ImGui::Separator();
	}

	const RollbackStats::Sample& latest = RollbackStats::GetLatest();

	if (ImGui::BeginTable("##netplaynow", 2, ImGuiTableFlags_SizingFixedFit))
	{
		Metric("Netplay frame", "%d", latest.frame);
		Metric("Frames re-simulated this match", "%d", latest.resimulated);
		Metric("Re-simulated per second", "%.1f", latest.resimulatedPerSecond);
		Metric("Ping (GGPO)", latest.ping > 0 ? "%d ms" : "-", latest.ping);
		DrawMeasuredPings();
		Metric("Frames behind (you)", "%d", latest.localFramesBehind);
		Metric("Frames behind (them)", "%d", latest.remoteFramesBehind);
		Metric("Send queue", "%d", latest.sendQueue);
		Metric("Sent", "%d kbps", latest.kbpsSent);
		Metric("Frame time", "%.2f ms", latest.frameMs);
		ImGui::EndTable();
	}

	UiText::Help("These numbers come straight from GGPO. "
		"GGPO's ping can read up to 2 frames (about 33 ms) higher than Steam's.");

	ImGui::Separator();

	const int liveCount = RollbackStats::LiveCount();

	if (liveCount > 1)
	{
		std::vector<float> resimulated;
		std::vector<float> pings;
		resimulated.reserve(liveCount);
		pings.reserve(liveCount);

		for (int i = 0; i < liveCount; ++i)
		{
			const RollbackStats::Sample& sample = RollbackStats::Live(i);
			resimulated.push_back(sample.resimulatedPerSecond);
			pings.push_back(static_cast<float>(sample.ping));
		}

		char overlay[64] = {};
		sprintf_s(overlay, "now %.1f/s", latest.resimulatedPerSecond);
		PlotSeries("Re-simulated/s", resimulated, Maximum(resimulated, 5.0f), overlay);

		sprintf_s(overlay, "now %d ms", latest.ping);
		PlotSeries("Ping", pings, Maximum(pings, 60.0f), overlay);
	}
	else
	{
		UiText::Muted("Nothing sampled yet.");
	}

	ImGui::Separator();
	DrawStartCapture();
}

void NetplayWindow::DrawMeasuredPings()
{
	SteamLink::Sample steam = {};
	SteamLink::Take(steam);

	Metric("Ping (Steam)", steam.valid && steam.ping > 0 ? "%d ms" : "-", steam.ping);

	if (!RoundTripSmoothing::IsPrimed())
		return;

	Metric("Round trip for the time sync", "%.0f ms, %d frames", RoundTripSmoothing::SmoothedMs(),
		RoundTripSmoothing::Frames());
}

void NetplayWindow::DrawStartCapture()
{
	ImGui::TextUnformatted("Match start");
	UiText::Help("Rollback in the first 15 seconds of an online match.");

	const int count = RollbackStats::StartCount();

	if (count <= 1)
	{
		UiText::Muted("Nothing captured yet. It fills in when an online match starts.");
		return;
	}

	std::vector<float> resimulated;
	resimulated.reserve(count);

	int firstFiveSeconds = 0;
	int afterFiveSeconds = 0;
	const int fiveSecondMark = 300;

	int previous = 0;

	for (int i = 0; i < count; ++i)
	{
		const RollbackStats::Sample& sample = RollbackStats::Start(i);
		resimulated.push_back(sample.resimulatedPerSecond);

		const int delta = sample.resimulated - previous;
		previous = sample.resimulated;

		if (delta <= 0)
			continue;

		if (i < fiveSecondMark)
			firstFiveSeconds += delta;
		else
			afterFiveSeconds += delta;
	}

	char overlay[64] = {};
	sprintf_s(overlay, "%d frames captured%s", count,
		RollbackStats::StartCaptureComplete() ? "" : " (filling)");

	PlotSeries("Start re-simulated/s", resimulated, Maximum(resimulated, 5.0f), overlay);

	ImGui::Text("First 5 s: %d re-simulated frames     After that: %d", firstFiveSeconds, afterFiveSeconds);

	if (firstFiveSeconds > afterFiveSeconds * 2 && afterFiveSeconds >= 0)
		UiText::Warn("Most re-simulation came at the start. The connection settled after that.");

	if (ImGui::Button("Capture the next match start"))
		RollbackStats::ClearStartCapture();
}

void NetplayWindow::DrawReplayUpload()
{
	ImGui::TextUnformatted("Replay upload");
	UiText::Help("Normally the game freezes until the replay finishes uploading. "
		"With this on, it uploads in the background and you move on right away.");

	bool background = g_modVals.backgroundReplayUpload;

	if (ImGui::Checkbox("Upload replays in the background", &background))
	{
		g_modVals.backgroundReplayUpload = background;
		Settings::SaveInt("Netplay", "BackgroundReplayUpload", background ? 1 : 0);
	}

	const int pending = BackgroundUpload::Pending();

	if (pending > 0)
		ImGui::Text("%d upload(s) still running.", pending);
}

void NetplayWindow::DrawNetcodeTab()
{
	if (!m_netcodeRead)
	{
		m_netcode = RunningNetcode();
		m_netcodeRead = true;
	}

	UiText::Help("These change how long your game waits for the opponent. "
		"They work against anyone, with or without the mod.");

	DrawInputDelay();
	ImGui::Separator();

	DrawNetcodeStatus();
	ImGui::Separator();

	bool changed = DrawNetcodeOptions();

	ImGui::Separator();
	ImGui::BeginDisabled(NetcodeChoice::IsGameOwn(m_netcode));

	if (ImGui::Button("Use the game's own netcode"))
	{
		m_netcode = NetcodeChoice::GameOwn();
		changed = true;
	}

	ImGui::EndDisabled();

	if (changed)
		SaveNetcode(m_netcode);
}

void NetplayWindow::DrawInputDelay()
{
	int frames = InputDelay::Frames();

	if (frames == InputDelay::kUnread)
	{
		UiText::Warn("Input delay can't be read on this game version.");
		return;
	}

	const bool canChange = InputDelay::CanChange();

	ImGui::BeginDisabled(!canChange);
	Ui::SetItemWidth(kSliderWidth);

	if (ImGui::SliderInt("Input delay", &frames, InputDelayRule::kFewestFrames, InputDelayRule::kMostFrames,
		"%d frames", ImGuiSliderFlags_AlwaysClamp))
	{
		InputDelay::SetFrames(frames);
	}

	ImGui::EndDisabled();
	UiText::Help("Same as the game's own Input lag option. More delay means fewer rollbacks but slower controls.\n"
		"Applies from your next match.");

	if (!canChange)
		UiText::Muted("Can't change during a match.");
}

bool NetplayWindow::DrawNetcodeOptions()
{
	bool changed = ImGui::Checkbox("Smooth the round trip the time sync reads", &m_netcode.smoothRoundTrip);
	UiText::Help("A shaky ping makes the game wait when it doesn't need to. This evens the ping out first.\n"
		"Game default: off.");

	Ui::SetItemWidth(kSliderWidth);
	ImGui::SliderInt("Time sync check", &m_netcode.timeSyncInterval, TimeSyncTuning::kShortestInterval,
		TimeSyncTuning::kGameInterval, "every %d frames", ImGuiSliderFlags_AlwaysClamp);
	changed |= ImGui::IsItemDeactivatedAfterEdit();
	UiText::Help("How often the game checks if you're ahead of the opponent and waits for them.\n"
		"Game default: every 240 frames (4 s).");

	changed |= ImGui::Checkbox("Keep the game's follow-up holds", &m_netcode.timeSyncTail);
	UiText::Help("After waiting once, the game keeps waiting a bit more for a while. Off removes those extra waits.\n"
		"Game default: on.");

	changed |= ImGui::Checkbox("Hold for half the lead, as stock GGPO", &m_netcode.timeSyncHalveGap);
	UiText::Help("The game waits out your whole lead, so both sides can end up taking turns waiting. "
		"This waits only half, like normal GGPO.\nGame default: off.");

	return changed;
}

void NetplayWindow::DrawNetcodeStatus()
{
	const NetcodeChoice::Values running = RunningNetcode();

	if (NetcodeChoice::IsGameOwn(running))
		UiText::Muted("Running now: the game's own netcode.");
	else
		UiText::Good("Running now: the mod's netcode changes.");

	if (!NetcodeChoice::Same(running, m_netcode))
		UiText::Warn("Restart the game to use these settings. The netcode changes are put in place when the game starts.");
}

void NetplayWindow::DrawNetworkLogTab()
{
	ImGui::TextUnformatted("Network log");
	UiText::Help("Saves connection info to a file. Send it when you report a connection problem.");

	bool logging = NetLog::IsEnabled();

	if (ImGui::Checkbox("Write the network log", &logging))
	{
		NetLog::SetEnabled(logging);
		g_modVals.netLog = logging;
		Settings::SaveInt("Netplay", "NetLog", logging ? 1 : 0);
	}

	if (NetLog::Path()[0] != 0)
		UiText::Muted("%s", NetLog::Path());

	if (NetLog::Dropped() > 0)
		UiText::Warn("%u line(s) were dropped because they came faster than the file was written.", NetLog::Dropped());

	bool ggpo = GgpoLogCapture::IsEnabled();

	if (ImGui::Checkbox("Include GGPO's own lines", &ggpo))
	{
		GgpoLogCapture::SetEnabled(ggpo);
		g_modVals.netLogGgpo = GgpoLogCapture::IsEnabled();
		Settings::SaveInt("Netplay", "CaptureGgpoLog", g_modVals.netLogGgpo ? 1 : 0);
	}

	UiText::Help("Adds GGPO's own messages: syncing, lost packets, disconnects. "
		"Only needed for detailed reports. Stops at 8 MB.");

	UiText::Muted("%s", GgpoLogCapture::StatusText());

	if (NetLog::IsOverBudget())
		UiText::Warn("The file reached 8 MB, so GGPO's lines have stopped. The mod's own carry on.");

	ImGui::Separator();
	ImGui::TextUnformatted("How the mod stays out of the connection");

	UiText::Muted("Steam listener: %s, %d event kind(s)", SteamWatch::IsRegistered() ? "on" : "not yet",
		SteamWatch::Registered());
	UiText::Muted("Steam work thread: %s, slowest job %.1f ms", NetWorker::IsRunning() ? "running" : "stopped",
		NetWorker::SlowestJobMs());
	UiText::Muted("Mod packets waiting: %d, sent to the opponent this match: %d B", ModChannel::Queued(),
		NetGate::PeerBytesThisSession());

	ImGui::Separator();
	ImGui::TextUnformatted("Connection");

	const NetLink::Snapshot& link = NetLink::Current();

	if (!link.hasPeer)
	{
		UiText::Muted(NetLink::IsBlind() ? "The GGPO session could not be read on this game version."
			: "No match connected.");
		return;
	}

	SteamLink::Sample steam = {};
	SteamLink::Take(steam);

	if (!ImGui::BeginTable("##connection", 2, ImGuiTableFlags_SizingFixedFit))
		return;

	Metric("Opponent", "%s", SteamNames::Resolve(link.peer.id).c_str());
	Metric("GGPO endpoint", "%s", NetLink::StateName(link.peer.state));
	Metric("GGPO ping", "%d ms", link.peer.ping);
	Metric("Inputs not yet acknowledged", "%d", link.peer.pending);

	if (steam.valid && steam.peer == link.peer.id)
	{
		Metric("Route", "%s", steam.usingRelay ? "Steam relay" : "direct");
		Metric("Steam send queue", "%d B, %d packet(s)", steam.bytesQueued, steam.packetsQueued);
		Metric("Steam ping", steam.messagesRead && steam.ping > 0 ? "%d ms" : "-", steam.ping);
		Metric("Link quality", "%.0f%% here, %.0f%% there", steam.qualityLocal * 100.0f, steam.qualityRemote * 100.0f);
		Metric("Relay network", "%s", SteamLink::AvailabilityName(steam.relayAvailability));
	}

	ImGui::EndTable();
}

void NetplayWindow::DrawRoomTab()
{
	ImGui::TextUnformatted("During a match");
	UiText::Help("During a match the mod leaves the room and the connection alone. Always on.");

	if (NetGate::MayTouchRoom())
		UiText::Muted("No match connected.");
	else
		UiText::Warn("A match is connected. Room features are paused.");

	ImGui::Separator();

	ImGui::TextUnformatted("Ghost members");
	UiText::Help("Players who crash, alt-F4 or get kicked stay stuck in the room list. This removes them.\n"
		"Off by default.");

	bool fix = RoomRoster::IsFixEnabled();
	if (ImGui::Checkbox("Remove players who disconnect or get kicked", &fix))
	{
		RoomRoster::SetFixEnabled(fix);
		g_modVals.roomRosterFix = fix;
		Settings::SaveInt("Netplay", "RoomRosterFix", fix ? 1 : 0);
	}

	if (RoomRoster::IsHooked())
		UiText::Good("Active. %d ghost(s) removed.", RoomRoster::GetGhostsPrevented());
	else
		UiText::Muted("%s", RoomRoster::GetStatusText());

	ImGui::Separator();

	ImGui::TextUnformatted("Who in the room has the mod");
	UiText::Help("Shows who in the room has the mod. Sends nothing.");

	if (!ModPresence::InRoom())
	{
		UiText::Muted("Join a room to see this.");
	}
	else
	{
		UiText::Good("%d of %d in this room have the mod", ModPresence::ModCount(), ModPresence::RoomSize());

		ModPresence::Member member = {};

		for (int i = 0; ModPresence::MemberAt(i, member); ++i)
		{
			if (member.id == 0)
				continue;

			const std::string name = SteamNames::Resolve(member.id);

			if (member.hasMod)
				UiText::Good("  %s (%s)", name.c_str(), member.version);
			else
				UiText::Muted("  %s (no mod)", name.c_str());
		}
	}

	ImGui::Separator();

	ImGui::TextUnformatted("Your opponent's mod");
	UiText::Help("Shows if your opponent has the mod. Palettes are only sent to opponents who do.");

	switch (ModHandshake::GetPeerState())
	{
	case ModHandshake::Peer_Modded:
		UiText::Good("%s", ModHandshake::GetStatusText());
		break;
	case ModHandshake::Peer_Unmodded:
		UiText::Warn("%s", ModHandshake::GetStatusText());
		break;
	default:
		UiText::Muted("%s", ModHandshake::GetStatusText());
		break;
	}

	ImGui::Separator();

	ImGui::TextUnformatted("Room ping");
	UiText::Help("Keeps the ping other players see for you accurate.\n"
		"Off by default. Never runs during a match.");

	bool republish = RoomPing::IsEnabled();
	if (ImGui::Checkbox("Update my ping location every 30 s", &republish))
	{
		RoomPing::SetEnabled(republish);
		g_modVals.republishPingLocation = republish;
		Settings::SaveInt("Netplay", "RepublishPingLocation", republish ? 1 : 0);
	}

	char pingStatus[160] = {};
	RoomPing::StatusText(pingStatus, sizeof(pingStatus));
	ImGui::TextUnformatted(pingStatus);

	if (NetLink::Lobby() != 0 && RoomPing::GetPublishCount() > 0)
	{
		ImGui::Text("Room %llu, last update %u s ago", static_cast<unsigned long long>(NetLink::Lobby()),
			RoomPing::GetSecondsSinceLastPublish());
	}

	ImGui::Separator();

	const int events = RoomRoster::EventCount();

	if (events == 0)
	{
		UiText::Muted("No one has joined or left yet.");
		return;
	}

	if (!ImGui::BeginTable("##roomevents", 3,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
		ImVec2(0.0f, Ui::Scaled(160.0f))))
	{
		return;
	}

	ImGui::TableSetupColumn("Member");
	ImGui::TableSetupColumn("Change");
	ImGui::TableSetupColumn("Handled");
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (int i = events - 1; i >= 0; --i)
	{
		RoomRoster::Event event = {};

		if (!RoomRoster::GetEvent(i, event))
			continue;

		char flags[96] = {};
		RoomRoster::DescribeFlags(event.rawFlags, flags, sizeof(flags));

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%llu", static_cast<unsigned long long>(event.user));
		ImGui::TableNextColumn();
		ImGui::Text("0x%02x %s", event.rawFlags, flags);
		ImGui::TableNextColumn();

		if (event.rewritten)
			UiText::Good("removed by the mod");
		else if ((event.rawFlags & RoomRoster::StateChange_Entered) != 0)
			ImGui::TextUnformatted("joined");
		else
			ImGui::TextUnformatted("game handled it");
	}

	ImGui::EndTable();
}

namespace {

void DrawWinRate(const OpponentLog::Entry& entry)
{
	const int games = entry.wins + entry.losses;

	if (games == 0)
	{
		ImGui::TextUnformatted("-");
		return;
	}

	ImGui::Text("%.0f%% (%d-%d)", entry.wins * 100.0f / games, entry.wins, entry.losses);
}

}

void NetplayWindow::DrawOpponentsTab()
{
	const int count = OpponentLog::Count();

	ImGui::Text("%d opponent(s) recorded", count);
	UiText::Help("Everyone you played online. Saved only on your PC.\n"
		"Win rate counts each game. Older matches aren't counted.");

	if (count == 0)
	{
		UiText::Muted("Play someone online and they appear here.");
		return;
	}

	if (!ImGui::BeginTable("##opponents", 6,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
	{
		return;
	}

	ImGui::TableSetupColumn("Name");
	ImGui::TableSetupColumn("Sets");
	ImGui::TableSetupColumn("Win rate");
	ImGui::TableSetupColumn("Last match");
	ImGui::TableSetupColumn("Ping");
	ImGui::TableSetupColumn("Last seen");
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (int i = 0; i < count; ++i)
	{
		const OpponentLog::Entry* entry = OpponentLog::Get(i);
		if (entry == nullptr)
			continue;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(entry->name[0] != '\0' ? entry->name : "[unknown]");

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%llu", static_cast<unsigned long long>(entry->steamId));

		ImGui::TableNextColumn();
		ImGui::Text("%d", entry->encounters);
		ImGui::TableNextColumn();
		DrawWinRate(*entry);
		ImGui::TableNextColumn();
		ImGui::Text("%s vs %s", CharacterName(entry->lastCharaLeft),
			CharacterName(entry->lastCharaRight));
		ImGui::TableNextColumn();

		if (entry->bestPing > 0)
			ImGui::Text("%d ms (best %d)", entry->lastPing, entry->bestPing);
		else
			ImGui::TextUnformatted("-");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(entry->lastSeen);
	}

	ImGui::EndTable();
}

namespace {

bool DrawPrivacyStatus(bool available, bool enabled, const char* status)
{
	if (!available)
	{
		UiText::Warn("%s", status);
		return false;
	}

	if (!enabled)
	{
		UiText::Muted("%s", status);
		return false;
	}

	UiText::Good("%s", status);
	return true;
}

}

void NetplayWindow::DrawPrivacyTab()
{
	ImGui::TextUnformatted("Other people's names");
	UiText::Help("Hides player names everywhere: rooms, lobby, player card, replays and the health bars.");

	bool censor = NameCensor::IsEnabled();

	if (ImGui::Checkbox("Hide other players' names", &censor))
	{
		NameCensor::SetEnabled(censor);
		g_modVals.censorNames = censor;
		Settings::SaveInt("Privacy", "CensorOpponentNames", censor ? 1 : 0);
	}

	char mask[NameCensor::kMaskMax + 1] = {};
	strncpy_s(mask, NameCensor::Mask(), _TRUNCATE);

	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));

	if (ImGui::InputText("Shown instead", mask, sizeof(mask),
		ImGuiInputTextFlags_EnterReturnsTrue) && mask[0] != 0)
	{
		NameCensor::SetMask(mask);
		Settings::SaveString("Privacy", "CensorMask", mask);
	}

	UiText::Help("Press Enter to save. It gets cut to the length of each name.");

	bool own = NameCensor::CoversOwnName();

	if (ImGui::Checkbox("Cover my own name too", &own))
	{
		NameCensor::SetCoversOwnName(own);
		g_modVals.censorOwnName = own;
		Settings::SaveInt("Privacy", "CensorMyName", own ? 1 : 0);
	}

	ImGui::Separator();

	if (!DrawPrivacyStatus(NameCensor::IsAvailable(), NameCensor::IsEnabled(), NameCensor::StatusText()))
		return;

	ImGui::Text("%d name(s) covered this run.", NameCensor::Count());
}

void NetplayWindow::DrawRoomNamePrivacy()
{
	ImGui::TextUnformatted("Room names");
	UiText::Help("Replaces room names in the room search with your \"Shown instead\" text. Only on your screen.");

	bool rooms = RoomNameCensor::IsEnabled();

	if (ImGui::Checkbox("Hide room names in the room search", &rooms))
	{
		RoomNameCensor::SetEnabled(rooms);
		g_modVals.censorRoomNames = rooms;
		Settings::SaveInt("Privacy", "CensorRoomNames", rooms ? 1 : 0);
	}

	if (!DrawPrivacyStatus(RoomNameCensor::IsAvailable(), RoomNameCensor::IsEnabled(),
		RoomNameCensor::StatusText()))
	{
		return;
	}

	ImGui::Text("%d room name(s) covered this run.", RoomNameCensor::Count());
}

void NetplayWindow::DrawIrPrivacy()
{
	ImGui::TextUnformatted("IR");
	UiText::Help("Hides every IR number, including the change after ranked matches. "
		"Only on your screen. Your IR still counts.");

	bool hide = IrHider::IsEnabled();

	if (ImGui::Checkbox("Hide IR", &hide))
	{
		IrHider::SetEnabled(hide);
		g_modVals.hideIr = hide;
		Settings::SaveInt("Privacy", "HideIr", hide ? 1 : 0);
	}

	if (!DrawPrivacyStatus(IrHider::IsAvailable(), IrHider::IsEnabled(), IrHider::StatusText()))
		return;

	ImGui::Text("%ld IR number(s) hidden this run.", IrHider::NumbersHidden());
}

namespace {

const char* ViewerStateText(const SpectateHost::Viewer& viewer)
{
	if (viewer.watching)
		return "watching";

	switch (viewer.state)
	{
	case SpectateHost::Viewer_Pending:
		return "wants to watch";
	case SpectateHost::Viewer_Kicked:
		return "removed";
	default:
		return "waiting for your next match";
	}
}

void DrawViewerRow(const SpectateHost::Viewer& viewer)
{
	char key[32] = {};
	sprintf_s(key, "%llu", static_cast<unsigned long long>(viewer.id));

	ImGui::PushID(key);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(SteamNames::Resolve(viewer.id).c_str());

	ImGui::TableNextColumn();
	ImGui::TextUnformatted(ViewerStateText(viewer));

	ImGui::TableNextColumn();

	if (viewer.state == SpectateHost::Viewer_Accepted)
	{
		if (ImGui::SmallButton("Kick"))
			SpectateHost::Kick(viewer.id);

		ImGui::PopID();
		return;
	}

	if (ImGui::SmallButton("Let in"))
		SpectateHost::Approve(viewer.id);

	ImGui::SameLine();

	if (ImGui::SmallButton(viewer.state == SpectateHost::Viewer_Pending ? "Decline" : "Forget"))
		SpectateHost::Forget(viewer.id);

	ImGui::PopID();
}

}

void NetplayWindow::DrawSpectateTab()
{
	DrawSpectateHost();
	ImGui::Separator();
	DrawSpectateWatch();
}

void NetplayWindow::DrawSpectateHost()
{
	ImGui::TextUnformatted("Let people watch you");
	UiText::Help("Lets Steam friends with the mod watch your online matches. They join at your next match.");

	bool allowed = SpectateHost::IsAllowed();

	if (ImGui::Checkbox("Allow spectators", &allowed))
		SpectateHost::SetAllowed(allowed);

	int most = SpectateHost::MaxViewers();
	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));

	if (ImGui::SliderInt("Viewers at most", &most, 1, SpectateHost::kMostViewers))
		SpectateHost::SetMaxViewers(most);

	UiText::Muted("%s", SpectateHost::StatusText());

	std::vector<SpectateHost::Viewer> viewers;
	SpectateHost::Snapshot(viewers);

	if (viewers.empty() || !ImGui::BeginTable("##viewers", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		return;

	ImGui::TableSetupColumn("Viewer");
	ImGui::TableSetupColumn("State");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();

	for (const SpectateHost::Viewer& viewer : viewers)
		DrawViewerRow(viewer);

	ImGui::EndTable();
}

void NetplayWindow::DrawSpectateWatch()
{
	ImGui::TextUnformatted("Watch someone");
	UiText::Help("Pick a friend to watch. You can't be in your own room while watching.");

	std::vector<SteamFriends::Friend> friends;
	SpectateViewer::Friends(friends);

	if (friends.empty())
		UiText::Muted("None of your Steam friends allow spectators right now.");

	for (const SteamFriends::Friend& buddy : friends)
	{
		ImGui::PushID(buddy.value);

		if (ImGui::SmallButton("Watch"))
			SpectateViewer::Watch(buddy.id);

		ImGui::SameLine();
		ImGui::TextUnformatted(buddy.name);
		ImGui::PopID();
	}

	if (SpectateViewer::GetState() == SpectateViewer::State_Idle)
	{
		UiText::Muted("%s", SpectateViewer::StatusText());
		return;
	}

	ImGui::Text("%s: %s", SteamNames::Resolve(SpectateViewer::Host()).c_str(), SpectateViewer::StatusText());

	if (ImGui::Button("Stop watching"))
		SpectateViewer::Leave();
}
