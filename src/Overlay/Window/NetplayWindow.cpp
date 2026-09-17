#include "Overlay/Window/NetplayWindow.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Game/CharaTables.h"
#include "Game/NameCensor.h"
#include "Game/RoomNameCensor.h"
#include "Game/OpponentLog.h"
#include "Network/RollbackStats.h"
#include "Game/SteamNames.h"
#include "Network/GgpoLogCapture.h"
#include "Network/ModChannel.h"
#include "Network/ModHandshake.h"
#include "Network/ModPresence.h"
#include "Network/NetGate.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/NetWorker.h"
#include "Network/RoomPing.h"
#include "Network/RoomRoster.h"
#include "Network/SteamLink.h"
#include "Network/SteamWatch.h"
#include "Network/SpectateHost.h"
#include "Network/SpectateViewer.h"
#include "Overlay/UiScale.h"
#include "Overlay/UiText.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace {

constexpr float kDefaultWidth = 720.0f;
constexpr float kDefaultHeight = 560.0f;
constexpr float kPlotHeight = 70.0f;

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
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Rollback"))
	{
		DrawRollbackTab();
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
		Metric("Rollbacks this match", "%d", latest.rollbacks);
		Metric("Rollbacks per second", "%.1f", latest.rollbacksPerSecond);
		Metric("Ping", latest.ping > 0 ? "%d ms" : "-", latest.ping);
		Metric("Frames behind (you)", "%d", latest.localFramesBehind);
		Metric("Frames behind (them)", "%d", latest.remoteFramesBehind);
		Metric("Send queue", "%d", latest.sendQueue);
		Metric("Sent", "%d kbps", latest.kbpsSent);
		Metric("Frame time", "%.2f ms", latest.frameMs);
		ImGui::EndTable();
	}

	UiText::Help("Ping, frames behind and the send queue are GGPO's own numbers, read from memory. "
		"The mod never calls into the netcode to get them.");

	ImGui::Separator();

	const int liveCount = RollbackStats::LiveCount();

	if (liveCount > 1)
	{
		std::vector<float> rollbacks;
		std::vector<float> pings;
		rollbacks.reserve(liveCount);
		pings.reserve(liveCount);

		for (int i = 0; i < liveCount; ++i)
		{
			const RollbackStats::Sample& sample = RollbackStats::Live(i);
			rollbacks.push_back(sample.rollbacksPerSecond);
			pings.push_back(static_cast<float>(sample.ping));
		}

		char overlay[64] = {};
		sprintf_s(overlay, "now %.1f/s", latest.rollbacksPerSecond);
		PlotSeries("Rollbacks/s", rollbacks, Maximum(rollbacks, 5.0f), overlay);

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

void NetplayWindow::DrawStartCapture()
{
	ImGui::TextUnformatted("Match start");
	UiText::Help("Rollbacks from the first 15 seconds online, saved once so you can look at them "
		"after the match.");

	const int count = RollbackStats::StartCount();

	if (count <= 1)
	{
		UiText::Muted("Nothing captured yet. It fills in when an online match starts.");
		return;
	}

	std::vector<float> rollbacks;
	rollbacks.reserve(count);

	int firstFiveSeconds = 0;
	int afterFiveSeconds = 0;
	const int fiveSecondMark = 300;

	int previous = 0;

	for (int i = 0; i < count; ++i)
	{
		const RollbackStats::Sample& sample = RollbackStats::Start(i);
		rollbacks.push_back(sample.rollbacksPerSecond);

		const int delta = sample.rollbacks - previous;
		previous = sample.rollbacks;

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

	PlotSeries("Start rollbacks/s", rollbacks, Maximum(rollbacks, 5.0f), overlay);

	ImGui::Text("First 5 s: %d rollbacks     After that: %d", firstFiveSeconds, afterFiveSeconds);

	if (firstFiveSeconds > afterFiveSeconds * 2 && afterFiveSeconds >= 0)
		UiText::Warn("Most rollbacks came at the start. The connection settled after that.");

	if (ImGui::Button("Capture the next match start"))
		RollbackStats::ClearStartCapture();
}

void NetplayWindow::DrawNetworkLogTab()
{
	ImGui::TextUnformatted("Network log");
	UiText::Help("Writes what the game's connection and the mod's network code do to its own file: "
		"every Steam event, every mod packet, and one line per second during a match. Send that file "
		"with any connection report.");

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

	UiText::Help("For a detailed report only. It catches the text GGPO writes about syncing, lost "
		"packets and disconnects, which the game normally throws away. It hooks two of GGPO's log "
		"functions and nothing else. The lines it writes every frame are counted once a second "
		"instead, and it stops once the file reaches 8 MB.");

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
	UiText::Help("While a match is connected the mod writes nothing to the room and never touches "
		"the game's connection. This is always on.");

	if (NetGate::MayTouchRoom())
		UiText::Muted("No match connected.");
	else
		UiText::Warn("A match is connected. Room features are paused.");

	ImGui::Separator();

	ImGui::TextUnformatted("Ghost members");
	UiText::Help("The game only removes players who leave the room normally. Anyone who "
		"disconnects, alt-F4s, or gets kicked or banned stays in the room list forever. This "
		"removes them too. It changes how the game handles its own room, so it is off by default.");

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
	UiText::Help("Players with the mod leave a marker on themselves in the room. This reads those "
		"markers, so it sends nothing.");

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
	UiText::Help("When a match connects, players with the mod say hello and share which patch and "
		"battle data they have. Palettes are only sent to an opponent who said hello back.");

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
	UiText::Help("Keeps your ping location fresh, so the ping other players see for you stays "
		"accurate. It writes a key the game owns, so it is off by default and never runs during a match.");

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

void NetplayWindow::DrawOpponentsTab()
{
	const int count = OpponentLog::Count();

	ImGui::Text("%d opponent(s) recorded", count);
	UiText::Help("A list of everyone you played online, saved on your PC. Steam keeps no such "
		"record for this game, so this is the only one.");

	if (count == 0)
	{
		UiText::Muted("Play someone online and they appear here.");
		return;
	}

	if (!ImGui::BeginTable("##opponents", 5,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
	{
		return;
	}

	ImGui::TableSetupColumn("Name");
	ImGui::TableSetupColumn("Sets");
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

void NetplayWindow::DrawPrivacyTab()
{
	ImGui::TextUnformatted("Other people's names");
	UiText::Help("Hides player names everywhere the game shows them: the room list, the lobby, the "
		"player card, the replay list and above the health bars.");

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

	UiText::Help("Press Enter to save it. The replacement is cut to the length of the name it "
		"covers, so a two letter name shows only two letters of it.");

	bool own = NameCensor::CoversOwnName();

	if (ImGui::Checkbox("Cover my own name too", &own))
	{
		NameCensor::SetCoversOwnName(own);
		g_modVals.censorOwnName = own;
		Settings::SaveInt("Privacy", "CensorMyName", own ? 1 : 0);
	}

	ImGui::Separator();

	if (!NameCensor::IsAvailable())
	{
		UiText::Warn("%s", NameCensor::StatusText());
		return;
	}

	if (!NameCensor::IsEnabled())
	{
		UiText::Muted("%s", NameCensor::StatusText());
		return;
	}

	UiText::Good("%s", NameCensor::StatusText());
	ImGui::Text("%d name(s) covered this run.", NameCensor::Count());
}

void NetplayWindow::DrawRoomNamePrivacy()
{
	ImGui::TextUnformatted("Room names");
	UiText::Help("Shows every room in the player match room search with the text in Shown instead. "
		"Only your screen changes. Your own room keeps its name for everyone else.");

	bool rooms = RoomNameCensor::IsEnabled();

	if (ImGui::Checkbox("Hide room names in the room search", &rooms))
	{
		RoomNameCensor::SetEnabled(rooms);
		g_modVals.censorRoomNames = rooms;
		Settings::SaveInt("Privacy", "CensorRoomNames", rooms ? 1 : 0);
	}

	if (!RoomNameCensor::IsAvailable())
	{
		UiText::Warn("%s", RoomNameCensor::StatusText());
		return;
	}

	if (!RoomNameCensor::IsEnabled())
	{
		UiText::Muted("%s", RoomNameCensor::StatusText());
		return;
	}

	UiText::Good("%s", RoomNameCensor::StatusText());
	ImGui::Text("%d room name(s) covered this run.", RoomNameCensor::Count());
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
	UiText::Help("Players with the mod can watch your online matches without joining your room. Your "
		"Steam friends see you in their list, and anyone you give your code to can watch too. They join "
		"at the start of your next match.");

	bool allowed = SpectateHost::IsAllowed();

	if (ImGui::Checkbox("Allow spectators", &allowed))
		SpectateHost::SetAllowed(allowed);

	const char* const code = SpectateHost::Code();

	if (code[0] != 0)
	{
		ImGui::Text("Your code: %s", code);
		ImGui::SameLine();

		if (ImGui::SmallButton("Copy"))
			ImGui::SetClipboardText(code);
	}

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
	UiText::Help("Pick a Steam friend who allows spectators, or type the code someone gave you. You "
		"cannot be in a room of your own while you watch.");

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

	ImGui::SetNextItemWidth(Ui::Scaled(200.0f));
	ImGui::InputTextWithHint("##spectatecode", "XXXX-XXXX-XXXXX", m_spectateCode, sizeof(m_spectateCode));
	ImGui::SameLine();

	if (ImGui::Button("Watch code"))
		SpectateViewer::WatchCode(m_spectateCode);

	if (SpectateViewer::GetState() == SpectateViewer::State_Idle)
	{
		UiText::Muted("%s", SpectateViewer::StatusText());
		return;
	}

	ImGui::Text("%s: %s", SteamNames::Resolve(SpectateViewer::Host()).c_str(), SpectateViewer::StatusText());

	if (ImGui::Button("Stop watching"))
		SpectateViewer::Leave();
}
