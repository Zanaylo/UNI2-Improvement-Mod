#include "Network/RoundLog.h"

#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/RoundEvents.h"
#include "Network/SideReadout.h"

namespace {

RoundEvents g_events;

}

void RoundLog::OnFrame()
{
	const NetLink::Snapshot& link = NetLink::Current();

	if (!NetLink::InSession(link))
	{
		g_events.Reset();
		return;
	}

	const RoundEvents::Event event = g_events.Update(SideReadout::ReadVitals(SideReadout::kFirstPlayer),
		SideReadout::ReadVitals(SideReadout::kSecondPlayer));

	if (event == RoundEvents::Event::KnockOut)
		NetLog::Write("round: knock-out at netplay frame %d", link.netplayFrame);

	if (event == RoundEvents::Event::RoundStart)
		NetLog::Write("round: start at netplay frame %d", link.netplayFrame);
}
