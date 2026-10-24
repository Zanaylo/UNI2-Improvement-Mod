#pragma once

#include <cstdint>
#include <vector>

namespace GameResults
{
	enum class Ending
	{
		Decided,
		OpponentLeft,
		ConnectionLost
	};

	struct Report
	{
		Ending ending;
		int winner;
		int localSide;
		uint64_t steamId[2];
		int chara[2];
		std::vector<uint8_t> record;
	};

	class Listener
	{
	public:
		virtual ~Listener() = default;

		virtual void OnGameEnded(const Report& report) = 0;
	};

	void AddListener(Listener* listener);

	const char* EndingName(Ending ending);

	void OnFrame();
}
