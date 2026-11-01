#pragma once

#include <string>
#include <vector>

namespace ModRules
{
	struct Mod
	{
		std::string folder;
		std::string id;
		bool enabled = false;
		std::vector<std::string> requirements;
		std::vector<std::string> conflicts;
		std::vector<std::string> above;
		std::vector<std::string> below;
	};

	enum class IssueKind
	{
		MissingRequirement,
		DisabledRequirement,
		Conflict,
		ShouldBeAbove,
		ShouldBeBelow
	};

	struct Issue
	{
		IssueKind kind;
		std::string other;
		int otherIndex;
	};

	int Find(const std::vector<Mod>& mods, const std::string& name);

	std::vector<std::vector<Issue>> Check(const std::vector<Mod>& topDown);
	bool Sorted(const std::vector<Mod>& topDown, std::vector<int>& order);
	std::vector<int> NeededBy(const std::vector<Mod>& topDown, int index);
}
