#include "Overlay/Window/NetplayWindow.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Game/CharaTables.h"
#include "Game/OpponentLog.h"
#include "Network/RollbackStats.h"
#include "Game/SteamNames.h"
#include "Network/ModPresence.h"
#include "Network/RoomPing.h"
#include "Network/OnlineSafety.h"
#include "Network/RoomRoster.h"
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

	if (ImGui::BeginTabItem("Rollback"))
	{
		DrawRollbackTab();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Room"))
	{
		DrawRoomTab();
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
		UiText::Muted("No netplay session. These numbers come from the game's own netplay counters "
			"and from GGPO, so they only exist during an online match.");
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

	UiText::Help("Ping and the two frame-advantage numbers come from GGPO's own GetNetworkStats, so "
		"they are measured, not an estimate from a ping location.");

	bool diagnostics = g_modVals.netplayDiagnostics;
	if (ImGui::Checkbox("Ask GGPO for ping and frame advantage", &diagnostics))
	{
		g_modVals.netplayDiagnostics = diagnostics;
		Settings::SaveInt("Netplay", "Diagnostics", diagnostics ? 1 : 0);
	}

	UiText::Help("Off by default, and off is the safe answer: the call lands on the netcode "
		"thread's own object from the render thread. Off, the rollback and frame counters still "
		"work - those are plain reads of the game's own globals and touch no session object.");

	if (g_modVals.netplayDiagnostics)
	{
		UiText::Warn("On. If a match drops or the room breaks, turn this off first and say whether "
			"it stopped.");
	}

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
	UiText::Help("The first 15 seconds of the session, captured once and kept, because a rolling "
		"window has always overwritten them by the time anyone looks.");

	const int count = RollbackStats::StartCount();

	if (count <= 1)
	{
		UiText::Muted("Nothing captured yet - it fills from the moment netplay starts.");
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
		UiText::Warn("The session is settling, not staying bad - the cost is at the start.");

	if (ImGui::Button("Capture the next start again"))
		RollbackStats::ClearStartCapture();
}

void NetplayWindow::DrawRoomTab()
{
	ImGui::TextUnformatted("While a match is connected");
	UiText::Help("Everything on this tab writes something the other people in the room receive, or "
		"reads an object the netcode owns. On, none of it runs once a session is up - that is what "
		"the mid-match disconnects were traced to. Off, each switch below decides for itself.");

	bool guarded = OnlineSafety::IsGuarded();

	if (ImGui::Checkbox("Hold the mod back during a session", &guarded))
	{
		OnlineSafety::SetGuarded(guarded);
		g_modVals.onlineSafety = guarded;
		Settings::SaveInt("Netplay", "SafeOnline", guarded ? 1 : 0);
	}

	if (OnlineSafety::InSession())
		UiText::Warn("%s", OnlineSafety::GetStatusText());
	else
		UiText::Muted("%s", OnlineSafety::GetStatusText());

	ImGui::Separator();

	ImGui::TextUnformatted("Ghost members");
	UiText::Help("The game removes a member only on an exact Left. Disconnected, Kicked and Banned "
		"are ignored, so an alt-F4 or a kick leaves the member in the room forever. This routes "
		"them all to the same handler.");

	bool fix = RoomRoster::IsFixEnabled();
	if (ImGui::Checkbox("Treat the member state change as a mask", &fix))
	{
		RoomRoster::SetFixEnabled(fix);
		g_modVals.roomRosterFix = fix;
		Settings::SaveInt("Netplay", "RoomRosterFix", fix ? 1 : 0);
	}

	if (RoomRoster::IsHooked())
		UiText::Good("Hooked. %d ghost(s) prevented.", RoomRoster::GetGhostsPrevented());
	else
		UiText::Warn("Not hooked: %s", RoomRoster::GetStatusText());

	ImGui::Separator();

	ImGui::TextUnformatted("Who else is running the mod");
	UiText::Help("Everyone running the mod publishes a marker on themselves in the room, so this "
		"counts the people here who have it without sending anything to anyone.");

	if (!ModPresence::InRoom())
	{
		UiText::Muted("Join a room to see this.");
	}
	else
	{
		UiText::Good("%s", ModPresence::GetStatusText());

		for (int i = 0; i < ModPresence::RoomSize(); ++i)
		{
			const uint64_t member = ModPresence::MemberAt(i);

			if (member == 0)
				continue;

			const std::string name = SteamNames::Resolve(member);

			if (ModPresence::HasMod(i))
				UiText::Good("  %s - %s", name.c_str(), ModPresence::VersionAt(i));
			else
				UiText::Muted("  %s - no mod", name.c_str());
		}
	}

	ImGui::Separator();

	ImGui::TextUnformatted("Room ping");
	UiText::Help("Republishes your ping location so the estimate other players see stops going "
		"stale. Unmodded clients in the room read the same key, so they benefit too.");

	bool republish = RoomPing::IsEnabled();
	if (ImGui::Checkbox("Republish my ping location every 30 s", &republish))
	{
		RoomPing::SetEnabled(republish);
		g_modVals.republishPingLocation = republish;
		Settings::SaveInt("Netplay", "RepublishPingLocation", republish ? 1 : 0);
	}

	ImGui::Text("%s", RoomPing::GetStatusText());

	if (RoomPing::InRoom())
	{
		ImGui::Text("Room %llu, last publish %u s ago",
			static_cast<unsigned long long>(RoomPing::GetLobbyId()),
			RoomPing::GetSecondsSinceLastPublish());
	}

	ImGui::Separator();

	const int events = RoomRoster::EventCount();

	if (events == 0)
	{
		UiText::Muted("No member changes seen yet.");
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
		const RoomRoster::Event& event = RoomRoster::GetEvent(i);

		char flags[96] = {};
		RoomRoster::DescribeFlags(event.rawFlags, flags, sizeof(flags));

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%llu", static_cast<unsigned long long>(event.user));
		ImGui::TableNextColumn();
		ImGui::Text("0x%02x %s", event.rawFlags, flags);
		ImGui::TableNextColumn();

		if (event.rewritten)
			UiText::Good("routed to Left");
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
	UiText::Help("Built locally from the SteamID on GGPO's rollback channel. Steam has no profile "
		"service for this game, so nothing here can come from anywhere else.");

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
