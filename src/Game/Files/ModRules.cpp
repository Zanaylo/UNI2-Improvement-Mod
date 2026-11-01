#include "Game/Files/ModRules.h"

#include "Core/Text/TextList.h"

#include <algorithm>

namespace {

using ModRules::Issue;
using ModRules::IssueKind;
using ModRules::Mod;

constexpr int kMissing = -1;

using Edges = std::vector<std::vector<int>>;

bool IsOn(const std::vector<Mod>& mods, int index)
{
	return index >= 0 && mods[index].enabled;
}

void Add(std::vector<Issue>& issues, IssueKind kind, const std::string& other, int otherIndex)
{
	const bool already = std::any_of(issues.begin(), issues.end(),
		[kind, &other](const Issue& issue) { return issue.kind == kind && TextList::SameNoCase(issue.other, other); });

	if (already)
		return;

	issues.push_back({ kind, other, otherIndex });
}

void CheckRequirements(const std::vector<Mod>& mods, int self, std::vector<Issue>& issues)
{
	for (const std::string& name : mods[self].requirements)
	{
		const int other = ModRules::Find(mods, name);

		if (other == kMissing)
			Add(issues, IssueKind::MissingRequirement, name, other);
		else if (!mods[other].enabled)
			Add(issues, IssueKind::DisabledRequirement, name, other);
		else if (other < self)
			Add(issues, IssueKind::ShouldBeAbove, name, other);
	}
}

void CheckConflicts(const std::vector<Mod>& mods, int self, std::vector<std::vector<Issue>>& issues)
{
	for (const std::string& name : mods[self].conflicts)
	{
		const int other = ModRules::Find(mods, name);

		if (!IsOn(mods, other) || other == self)
			continue;

		Add(issues[self], IssueKind::Conflict, name, other);
		Add(issues[other], IssueKind::Conflict, mods[self].id, self);
	}
}

void CheckOrder(const std::vector<Mod>& mods, int self, std::vector<Issue>& issues)
{
	for (const std::string& name : mods[self].above)
	{
		const int other = ModRules::Find(mods, name);

		if (IsOn(mods, other) && other < self)
			Add(issues, IssueKind::ShouldBeAbove, name, other);
	}

	for (const std::string& name : mods[self].below)
	{
		const int other = ModRules::Find(mods, name);

		if (IsOn(mods, other) && other > self)
			Add(issues, IssueKind::ShouldBeBelow, name, other);
	}
}

void Link(Edges& edges, std::vector<int>& incoming, int upper, int lower)
{
	if (upper == kMissing || lower == kMissing || upper == lower)
		return;

	std::vector<int>& out = edges[upper];

	if (std::find(out.begin(), out.end(), lower) != out.end())
		return;

	out.push_back(lower);
	++incoming[lower];
}

void LinkAll(const std::vector<Mod>& mods, Edges& edges, std::vector<int>& incoming)
{
	for (int self = 0; self < static_cast<int>(mods.size()); ++self)
	{
		for (const std::string& name : mods[self].requirements)
			Link(edges, incoming, self, ModRules::Find(mods, name));

		for (const std::string& name : mods[self].above)
			Link(edges, incoming, self, ModRules::Find(mods, name));

		for (const std::string& name : mods[self].below)
			Link(edges, incoming, ModRules::Find(mods, name), self);
	}
}

int FirstReady(const std::vector<int>& incoming, const std::vector<bool>& placed)
{
	for (int i = 0; i < static_cast<int>(incoming.size()); ++i)
	{
		if (!placed[i] && incoming[i] == 0)
			return i;
	}

	return kMissing;
}

}

int ModRules::Find(const std::vector<Mod>& mods, const std::string& name)
{
	for (int i = 0; i < static_cast<int>(mods.size()); ++i)
	{
		if (TextList::SameNoCase(mods[i].id, name) || TextList::SameNoCase(mods[i].folder, name))
			return i;
	}

	return kMissing;
}

std::vector<std::vector<Issue>> ModRules::Check(const std::vector<Mod>& topDown)
{
	std::vector<std::vector<Issue>> issues(topDown.size());

	for (int self = 0; self < static_cast<int>(topDown.size()); ++self)
	{
		if (!topDown[self].enabled)
			continue;

		CheckRequirements(topDown, self, issues[self]);
		CheckConflicts(topDown, self, issues);
		CheckOrder(topDown, self, issues[self]);
	}

	return issues;
}

bool ModRules::Sorted(const std::vector<Mod>& topDown, std::vector<int>& order)
{
	const size_t count = topDown.size();
	Edges edges(count);
	std::vector<int> incoming(count, 0);
	std::vector<bool> placed(count, false);

	LinkAll(topDown, edges, incoming);
	order.clear();

	for (size_t step = 0; step < count; ++step)
	{
		const int next = FirstReady(incoming, placed);

		if (next == kMissing)
			return false;

		placed[next] = true;
		order.push_back(next);

		for (const int lower : edges[next])
			--incoming[lower];
	}

	return true;
}

std::vector<int> ModRules::NeededBy(const std::vector<Mod>& topDown, int index)
{
	std::vector<int> needed;

	if (index < 0 || index >= static_cast<int>(topDown.size()))
		return needed;

	std::vector<bool> seen(topDown.size(), false);
	std::vector<int> pending = { index };
	seen[index] = true;

	for (size_t at = 0; at < pending.size(); ++at)
	{
		for (const std::string& name : topDown[pending[at]].requirements)
		{
			const int other = Find(topDown, name);

			if (other == kMissing || seen[other])
				continue;

			seen[other] = true;
			pending.push_back(other);

			if (!topDown[other].enabled)
				needed.push_back(other);
		}
	}

	return needed;
}
