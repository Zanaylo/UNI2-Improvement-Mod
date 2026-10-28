#pragma once

#include <string>
#include <vector>

namespace BgTable
{
	struct Text
	{
		std::string list;
		std::string names;
	};

	std::vector<int> Numbers(const std::string& list);
	bool Entry(const std::string& list, int number, std::string& out);
	void Put(std::string& list, int number, const std::string& entry);
	void Drop(std::string& list, int number);

	bool Listed(const std::string& list, std::vector<int>& out);
	void SetListed(std::string& list, const std::vector<int>& numbers);

	std::string DataFile(const std::string& list, int number);
	std::string Name(const std::string& list, int number);
	std::string Key(const std::string& list, int number);

	std::vector<int> Named(const std::string& names);
	bool HasName(const std::string& names, int number);
	bool NameLine(const std::string& names, int number, std::string& out);
	void PutNameLine(std::string& names, int number, const std::string& line);
	void SetName(std::string& names, int number, const std::string& name);
	void ClearName(std::string& names, int number);

	void Stack(Text& composed, const Text& layer);
	int TopListing(const std::vector<Text>& layers, std::vector<int>& outListed);
	Text Fragment(const Text& from, const std::vector<int>& numbers);
	Text Edits(const Text& old, const Text& base, const Text& made, const std::vector<int>& numbers);
	std::vector<int> DropKeyed(Text& text, const std::vector<std::string>& keys);
	std::vector<std::string> Keys(const std::string& list, const std::string& fallback,
		const std::vector<int>& numbers);
}
