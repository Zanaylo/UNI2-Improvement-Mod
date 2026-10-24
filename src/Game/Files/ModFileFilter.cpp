#include "Game/Files/ModFileFilter.h"

namespace {

constexpr char kStageFolder[] = "bg\\";
constexpr char kLanguagePrefix[] = "___";
constexpr char kSeparator = '\\';
constexpr size_t kStageFolderLength = sizeof(kStageFolder) - 1;
constexpr size_t kLanguagePrefixLength = sizeof(kLanguagePrefix) - 1;

std::string WithoutLanguageRoot(const std::string& key)
{
	if (key.compare(0, kLanguagePrefixLength, kLanguagePrefix) != 0)
		return key;

	const size_t separator = key.find(kSeparator);

	if (separator == std::string::npos)
		return key;

	return key.substr(separator + 1);
}

bool IsStageFile(const std::string& key)
{
	return WithoutLanguageRoot(key).compare(0, kStageFolderLength, kStageFolder) == 0;
}

}

bool ModFileFilter::Keeps(const std::string& key, bool stagesOn)
{
	if (stagesOn)
		return true;

	return !IsStageFile(key);
}
