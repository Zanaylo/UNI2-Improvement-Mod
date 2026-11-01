#include "Game/Files/ModLayout.h"

#include "Core/Text/TextList.h"

#include <cstring>

namespace {

constexpr char kSeparator = '\\';
constexpr const char* kLanguagePrefix = "___";
constexpr size_t kLanguagePrefixLength = 3;

const char* const kGameRoots[] = { "battleres", "bg", "bgm", "data", "dlc", "grpdat", "movie", "script", "se",
	"shader", "system" };

constexpr const char* kScriptRoot = "script\\";
constexpr const char* kDataRoot = "data\\";
const char* const kRuleExtensions[] = { ".ha6", ".txt" };
const char* const kCosmeticMarks[] = { "_se_", "_com_" };

std::string FirstFolder(const std::string& key)
{
	const size_t separator = key.find(kSeparator);

	return separator == std::string::npos ? std::string() : key.substr(0, separator);
}

std::string WithoutLanguageRoot(const std::string& key)
{
	if (key.compare(0, kLanguagePrefixLength, kLanguagePrefix) != 0)
		return key;

	const size_t separator = key.find(kSeparator);

	return separator == std::string::npos ? key : key.substr(separator + 1);
}

bool StartsWith(const std::string& text, const char* head)
{
	return text.compare(0, strlen(head), head) == 0;
}

bool EndsWith(const std::string& text, const char* tail)
{
	const size_t length = strlen(tail);

	return text.size() >= length && _stricmp(text.c_str() + text.size() - length, tail) == 0;
}

bool IsRuleFile(const std::string& path)
{
	for (const char* extension : kRuleExtensions)
	{
		if (!EndsWith(path, extension))
			continue;

		for (const char* mark : kCosmeticMarks)
		{
			if (path.find(mark) != std::string::npos)
				return false;
		}

		return true;
	}

	return false;
}

}

bool ModLayout::IsGameRoot(const std::string& folder)
{
	if (StartsWith(folder, kLanguagePrefix))
		return true;

	for (const char* root : kGameRoots)
	{
		if (TextList::SameNoCase(folder, root))
			return true;
	}

	return false;
}

std::string ModLayout::WrapperFolder(const std::vector<std::string>& keys)
{
	std::string shared;

	for (const std::string& key : keys)
	{
		const std::string folder = FirstFolder(key);

		if (folder.empty())
			continue;

		if (IsGameRoot(folder))
			return std::string();

		if (shared.empty())
			shared = folder;
		else if (!TextList::SameNoCase(shared, folder))
			return std::string();
	}

	return shared;
}

bool ModLayout::ChangesGameplay(const std::string& key)
{
	const std::string path = WithoutLanguageRoot(key);

	if (StartsWith(path, kScriptRoot))
		return true;

	return StartsWith(path, kDataRoot) && IsRuleFile(path);
}
