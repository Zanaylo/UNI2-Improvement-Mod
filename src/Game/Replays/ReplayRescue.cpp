#include "Game/Replays/ReplayRescue.h"

#include "Core/logger.h"
#include "Game/Replays/ReplayFiles.h"
#include "Network/GameResults.h"
#include "Network/NetLog.h"

#include <cstdio>
#include <string>

namespace {

const char* const kSideNames[] = { "P1", "P2" };

char g_status[256] = "no game has been interrupted";

std::string NoteFor(const GameResults::Report& report)
{
	if (report.ending != GameResults::Ending::OpponentLeft)
		return "connection-lost";

	const int quitter = 1 - report.winner;
	return std::string(kSideNames[quitter]) + "-left-" + kSideNames[report.winner] + "-wins";
}

class Rescuer final : public GameResults::Listener
{
public:
	void OnGameEnded(const GameResults::Report& report) override
	{
		if (report.ending == GameResults::Ending::Decided)
			return;

		if (report.record.empty())
		{
			sprintf_s(g_status, "a game was interrupted, but its replay could not be read");
			NetLog::Write("replay rescue: %s", g_status);
			return;
		}

		const std::string path = ReplayFiles::SaveRecord(report.record, NoteFor(report));

		if (path.empty())
		{
			_snprintf_s(g_status, _TRUNCATE, "an interrupted game could not be saved: %s", ReplayFiles::GetStatus());
			NetLog::Write("replay rescue: %s", g_status);
			return;
		}

		_snprintf_s(g_status, _TRUNCATE, "saved an interrupted game (%s) to %s",
			GameResults::EndingName(report.ending), path.c_str());
		NetLog::Write("replay rescue: %s", g_status);
		LOG("replay rescue: %s", g_status);
	}
};

Rescuer g_rescuer;

}

void ReplayRescue::Initialize()
{
	GameResults::AddListener(&g_rescuer);
}

const char* ReplayRescue::GetStatus()
{
	return g_status;
}
