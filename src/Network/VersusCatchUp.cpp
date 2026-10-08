#include "Network/VersusCatchUp.h"

#include "Core/Config/interfaces.h"
#include "Game/Display/FrameWaitSkip.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/FrameGapCatchUp.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"

#include <cstdint>

namespace {

constexpr int kAdvantageWindowFrames = 40;
constexpr int kStartGapFrames = 2;
constexpr int kSettleFrames = 120;

FrameGapCatchUp g_gap(kAdvantageWindowFrames, kStartGapFrames, kSettleFrames);
uint32_t g_session = 0;
int g_lastNetplayFrame = -1;

bool IsPlayingAVersusMatch(const NetLink::Snapshot& link)
{
	return g_modVals.versusCatchUp && link.backend == NetLink::Backend_Players && link.layoutValid &&
		link.hasPeer && link.peer.state == GameOffsets::kGgpoStateRunning && !link.synchronizing &&
		link.netplayActive && link.pacePending == 0 && FrameWaitSkip::IsMeasured();
}

void StandDown()
{
	g_gap.Reset();
	g_lastNetplayFrame = -1;
}

void Follow(uint32_t session)
{
	if (session == g_session)
		return;

	StandDown();
	g_session = session;
}

bool SimulationAdvanced(int netplayFrame)
{
	const bool advanced = g_lastNetplayFrame >= 0 && netplayFrame > g_lastNetplayFrame;
	g_lastNetplayFrame = netplayFrame;

	return advanced;
}

}

void VersusCatchUp::Update()
{
	const NetLink::Snapshot& link = NetLink::Current();

	if (!IsPlayingAVersusMatch(link))
	{
		StandDown();
		return;
	}

	Follow(link.session);

	if (!SimulationAdvanced(link.netplayFrame))
		return;

	const bool wasHurrying = g_gap.IsHurrying();

	if (!g_gap.Update(link.peer.localBehind, link.peer.remoteBehind))
		return;

	if (!wasHurrying)
	{
		NetLog::Write("versus catch-up: %d frame(s) behind the opponent at netplay frame %d, running them without "
			"the frame wait", g_gap.LastGapFrames(), link.netplayFrame);
	}

	FrameWaitSkip::SkipNextWait();
}
