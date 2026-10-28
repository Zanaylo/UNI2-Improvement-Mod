#pragma once

#include <string>
#include <vector>

namespace PickerOrder
{
	struct Keyed
	{
		int number;
		std::string key;
	};

	struct Choices
	{
		std::vector<std::string> order;
		std::vector<std::string> hidden;
		std::vector<std::string> added;
	};

	std::vector<int> Arrange(const std::vector<Keyed>& standard, const std::vector<Keyed>& defined,
		const Choices& choices);

	Choices Learn(const std::vector<Keyed>& standard, const std::vector<Keyed>& defined,
		const std::vector<std::string>& listed);

	void Show(Choices& choices, const std::string& key, bool shown, bool standard);

	std::vector<std::string> Split(const std::string& stored);
	std::string Join(const std::vector<std::string>& keys);

	void Place(std::vector<int>& listed, int number, const std::vector<int>& gameOrder);
	std::vector<int> Followed(const std::vector<int>& listed, const std::vector<int>& kept);
}
