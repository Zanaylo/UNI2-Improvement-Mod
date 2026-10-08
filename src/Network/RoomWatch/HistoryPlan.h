#pragma once

namespace HistoryPlan
{
	constexpr int kUnknownFrame = -1;

	int LastToSend(int confirmed, int acked, int firstLive, int window);
	bool IsDone(int acked, int firstLive);
}
