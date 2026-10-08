#include "Network/RoomWatch/HistoryPlan.h"

#include <algorithm>

int HistoryPlan::LastToSend(int confirmed, int acked, int firstLive, int window)
{
	const int last = (std::min)(confirmed, acked + window);

	if (firstLive == kUnknownFrame)
		return last;

	return (std::min)(last, firstLive - 1);
}

bool HistoryPlan::IsDone(int acked, int firstLive)
{
	return firstLive != kUnknownFrame && acked >= firstLive - 1;
}
