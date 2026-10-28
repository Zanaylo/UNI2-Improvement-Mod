#include "Game/Stages/PickerOrder.h"

#include <algorithm>

namespace {

bool Contains(const std::vector<int>& numbers, int number)
{
	return std::find(numbers.begin(), numbers.end(), number) != numbers.end();
}

int NeighbourInList(const std::vector<int>& listed, const std::vector<int>& gameOrder, size_t from, int step)
{
	for (int at = static_cast<int>(from) + step; at >= 0 && at < static_cast<int>(gameOrder.size()); at += step)
	{
		if (Contains(listed, gameOrder[at]))
			return gameOrder[at];
	}

	return -1;
}

size_t IndexOf(const std::vector<int>& numbers, int number)
{
	return static_cast<size_t>(std::find(numbers.begin(), numbers.end(), number) - numbers.begin());
}

bool Holds(const std::vector<std::string>& keys, const std::string& key)
{
	return std::find(keys.begin(), keys.end(), key) != keys.end();
}

void Remove(std::vector<std::string>& keys, const std::string& key)
{
	keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
}

void Keep(std::vector<std::string>& keys, const std::string& key)
{
	if (!Holds(keys, key))
		keys.push_back(key);
}

bool HasKey(const std::vector<PickerOrder::Keyed>& keyed, const std::string& key)
{
	return std::any_of(keyed.begin(), keyed.end(),
		[&key](const PickerOrder::Keyed& one) { return one.key == key; });
}

std::vector<PickerOrder::Keyed> Candidates(const std::vector<PickerOrder::Keyed>& standard,
	const std::vector<PickerOrder::Keyed>& defined, const PickerOrder::Choices& choices)
{
	std::vector<PickerOrder::Keyed> candidates;
	std::vector<int> numbers;

	const auto take = [&](const PickerOrder::Keyed& one)
	{
		if (Holds(choices.hidden, one.key) || Contains(numbers, one.number))
			return;

		candidates.push_back(one);
		numbers.push_back(one.number);
	};

	for (const PickerOrder::Keyed& one : standard)
		take(one);

	for (const std::string& key : choices.added)
	{
		for (const PickerOrder::Keyed& one : defined)
		{
			if (one.key == key)
				take(one);
		}
	}

	return candidates;
}

std::string Trimmed(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t");

	if (first == std::string::npos)
		return std::string();

	return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

}

std::vector<int> PickerOrder::Arrange(const std::vector<Keyed>& standard, const std::vector<Keyed>& defined,
	const Choices& choices)
{
	const std::vector<Keyed> candidates = Candidates(standard, defined, choices);

	std::vector<int> natural;

	for (const Keyed& one : candidates)
		natural.push_back(one.number);

	std::vector<int> arranged;

	for (const std::string& key : choices.order)
	{
		for (const Keyed& one : candidates)
		{
			if (one.key == key && !Contains(arranged, one.number))
				arranged.push_back(one.number);
		}
	}

	for (int number : natural)
		Place(arranged, number, natural);

	return arranged;
}

PickerOrder::Choices PickerOrder::Learn(const std::vector<Keyed>& standard, const std::vector<Keyed>& defined,
	const std::vector<std::string>& listed)
{
	Choices learnt;

	for (const std::string& key : listed)
	{
		if (!HasKey(defined, key))
			continue;

		Keep(learnt.order, key);

		if (!HasKey(standard, key))
			Keep(learnt.added, key);
	}

	for (const Keyed& one : standard)
	{
		if (!Holds(listed, one.key))
			Keep(learnt.hidden, one.key);
	}

	return learnt;
}

void PickerOrder::Show(Choices& choices, const std::string& key, bool shown, bool standard)
{
	if (shown)
	{
		Remove(choices.hidden, key);

		if (!standard)
			Keep(choices.added, key);

		return;
	}

	Remove(choices.added, key);

	if (standard)
		Keep(choices.hidden, key);
}

std::vector<std::string> PickerOrder::Split(const std::string& stored)
{
	std::vector<std::string> keys;
	size_t start = 0;

	while (start <= stored.size())
	{
		const size_t comma = stored.find(',', start);
		const size_t end = comma == std::string::npos ? stored.size() : comma;
		const std::string key = Trimmed(stored.substr(start, end - start));

		if (!key.empty())
			keys.push_back(key);

		start = end + 1;
	}

	return keys;
}

std::string PickerOrder::Join(const std::vector<std::string>& keys)
{
	std::string joined;

	for (const std::string& key : keys)
	{
		if (!joined.empty())
			joined += ", ";

		joined += key;
	}

	return joined;
}

void PickerOrder::Place(std::vector<int>& listed, int number, const std::vector<int>& gameOrder)
{
	if (Contains(listed, number))
		return;

	const size_t spot = IndexOf(gameOrder, number);

	if (spot == gameOrder.size())
	{
		listed.push_back(number);
		return;
	}

	const int previous = NeighbourInList(listed, gameOrder, spot, -1);

	if (previous >= 0)
	{
		listed.insert(listed.begin() + IndexOf(listed, previous) + 1, number);
		return;
	}

	const int next = NeighbourInList(listed, gameOrder, spot, 1);

	if (next >= 0)
	{
		listed.insert(listed.begin() + IndexOf(listed, next), number);
		return;
	}

	listed.push_back(number);
}

std::vector<int> PickerOrder::Followed(const std::vector<int>& listed, const std::vector<int>& kept)
{
	std::vector<int> followed = listed;

	for (int number : kept)
	{
		if (!Contains(followed, number))
			followed.push_back(number);
	}

	return followed;
}
