#include "Game/Stages/PackStages.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr const char* kStagePrefix = "bg\\bg";
constexpr const char* kModel = "bg.fbx.bin";
constexpr const char* kObjectRoot = "./bg/";
constexpr char kSeparator = '\\';
constexpr char kObjectSeparator = '/';
constexpr size_t kDigits = 3;

bool AllDigits(const std::string& text)
{
	return !text.empty() && std::all_of(text.begin(), text.end(),
		[](char letter) { return isdigit(static_cast<unsigned char>(letter)) != 0; });
}

std::string Lowered(const std::string& text)
{
	std::string lowered = text;
	std::transform(lowered.begin(), lowered.end(), lowered.begin(),
		[](char letter) { return static_cast<char>(tolower(static_cast<unsigned char>(letter))); });

	return lowered;
}

bool StageKey(const std::string& key, int& number, std::string& rest)
{
	const std::string prefix = kStagePrefix;

	if (key.compare(0, prefix.size(), prefix) != 0)
		return false;

	const size_t separator = key.find(kSeparator, prefix.size());

	if (separator == std::string::npos)
		return false;

	const std::string digits = key.substr(prefix.size(), separator - prefix.size());

	if (digits.size() != kDigits || !AllDigits(digits))
		return false;

	number = atoi(digits.c_str());
	rest = key.substr(separator + 1);

	return number > 0 && number <= PackStages::kHighestNumber;
}

PackStages::Folder* FolderOf(std::vector<PackStages::Folder>& folders, int number)
{
	const auto found = std::find_if(folders.begin(), folders.end(),
		[number](const PackStages::Folder& folder) { return folder.number == number; });

	return found == folders.end() ? nullptr : &*found;
}

bool AlreadyClaimed(const std::vector<PackStages::Claimed>& claimed, int number)
{
	return std::any_of(claimed.begin(), claimed.end(),
		[number](const PackStages::Claimed& one) { return one.number == number; });
}

}

std::vector<PackStages::Folder> PackStages::Find(const std::vector<std::string>& keys)
{
	std::vector<Folder> folders;

	for (const std::string& key : keys)
	{
		int number = 0;
		std::string rest;

		if (!StageKey(key, number, rest))
			continue;

		const bool ready = rest == kModel;
		Folder* const known = FolderOf(folders, number);

		if (known != nullptr)
		{
			known->ready = known->ready || ready;
			continue;
		}

		folders.push_back({ number, ready });
	}

	std::sort(folders.begin(), folders.end(),
		[](const Folder& a, const Folder& b) { return a.number < b.number; });

	return folders;
}

std::vector<PackStages::Claimed> PackStages::Claim(const std::vector<std::vector<Folder>>& packsTopDown,
	const std::function<bool(int)>& open)
{
	std::vector<Claimed> claimed;

	for (size_t pack = 0; pack < packsTopDown.size(); ++pack)
	{
		for (const Folder& folder : packsTopDown[pack])
		{
			if (!folder.ready || AlreadyClaimed(claimed, folder.number) || !open(folder.number))
				continue;

			claimed.push_back({ static_cast<int>(pack), folder.number });
		}
	}

	return claimed;
}

std::vector<std::string> PackStages::ForeignFolders(const std::string& objectList, int number)
{
	const std::string text = Lowered(objectList);
	const std::string root = kObjectRoot;
	const std::string home = FolderName(number);

	std::vector<std::string> foreign;

	for (size_t at = text.find(root); at != std::string::npos; at = text.find(root, at + root.size()))
	{
		const size_t start = at + root.size();
		const size_t close = text.find(kObjectSeparator, start);

		if (close == std::string::npos)
			break;

		const std::string folder = text.substr(start, close - start);

		if (folder == home || std::find(foreign.begin(), foreign.end(), folder) != foreign.end())
			continue;

		foreign.push_back(folder);
	}

	return foreign;
}

std::string PackStages::FolderName(int number)
{
	char name[16] = {};
	sprintf_s(name, "bg%03d", number);

	return name;
}
