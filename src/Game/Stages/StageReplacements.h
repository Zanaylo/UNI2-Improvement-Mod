#pragma once

#include <string>
#include <vector>

namespace StageReplacements
{
	struct Replacement
	{
		int number;
		std::string name;
		std::string note;
	};

	void Load();

	void Snapshot(std::vector<Replacement>& out);
	void Stale(std::vector<int>& out);
	bool Replaced(int number, std::string& name);

	void Forget(int number);
}
