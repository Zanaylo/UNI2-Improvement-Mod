#pragma once

#include <string>
#include <vector>

namespace ObjectList
{
	constexpr int kMostEntries = 99;

	struct Frame
	{
		std::string name;
		int wait;
	};

	struct Entry
	{
		int number;
		std::vector<Frame> frames;
		int prio;
		float start[3];
		int delay;
	};

	std::vector<Entry> Read(const std::string& text);
}
