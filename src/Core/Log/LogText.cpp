#include "Core/Log/LogText.h"

#include <cctype>
#include <cstring>

namespace {

constexpr const char* kGeneral = "General";
constexpr const char* kCrashName = "CRASH";
constexpr const char* kScope = "::";
constexpr const char* kErrorWord = "error";
constexpr int kMostSpaces = 1;

const char* const kWarnPhrases[] = { "could not", "cannot", "can't", "failed", "failure", "unable to" };
const char* const kHarmlessBeforeError[] = { "no ", "0 " };

const char* const kLevelNames[] = { "Debug", "Info", "Warning", "Error" };
const char* const kFileTags[] = { "[debug] ", "", "[warn] ", "[error] " };

bool IsNameLetter(char letter)
{
	return isalnum(static_cast<unsigned char>(letter)) != 0 || letter == '_' || letter == '-' || letter == '.';
}

std::string Lowered(const char* line)
{
	std::string lowered(line);

	for (char& letter : lowered)
		letter = static_cast<char>(tolower(static_cast<unsigned char>(letter)));

	return lowered;
}

bool IsWordAt(const std::string& text, size_t at, size_t length)
{
	const bool startsWord = at == 0 || !IsNameLetter(text[at - 1]);
	const size_t end = at + length;
	const bool endsWord = end >= text.size() || !isalpha(static_cast<unsigned char>(text[end]));

	return startsWord && endsWord;
}

bool FollowsHarmless(const std::string& text, size_t at)
{
	for (const char* harmless : kHarmlessBeforeError)
	{
		const size_t length = strlen(harmless);

		if (at >= length && text.compare(at - length, length, harmless) == 0)
			return true;
	}

	return false;
}

bool MentionsError(const std::string& lowered)
{
	const size_t length = strlen(kErrorWord);

	for (size_t at = lowered.find(kErrorWord); at != std::string::npos; at = lowered.find(kErrorWord, at + 1))
	{
		if (IsWordAt(lowered, at, length) && !FollowsHarmless(lowered, at))
			return true;
	}

	return false;
}

bool MentionsFailure(const std::string& lowered)
{
	for (const char* phrase : kWarnPhrases)
	{
		if (lowered.find(phrase) != std::string::npos)
			return true;
	}

	return false;
}

std::string Owner(const std::string& name)
{
	const size_t scope = name.find(kScope);

	return scope == std::string::npos ? name : name.substr(0, scope);
}

}

std::string LogText::Category(const char* line)
{
	if (line == nullptr || !isalpha(static_cast<unsigned char>(line[0])))
		return kGeneral;

	int spaces = 0;

	for (size_t at = 0; at <= kLongestCategory && line[at] != '\0'; ++at)
	{
		const char letter = line[at];
		const bool ends = letter == ':' && (line[at + 1] == ' ' || line[at + 1] == '\0');

		if (ends)
			return Owner(std::string(line, at));

		if (letter == ':' && line[at + 1] == ':')
		{
			++at;
			continue;
		}

		if (letter == ' ' && ++spaces <= kMostSpaces)
			continue;

		if (!IsNameLetter(letter))
			return kGeneral;
	}

	return kGeneral;
}

LogLevel LogText::Guess(const char* line)
{
	if (line == nullptr)
		return LogLevel::Info;

	if (Category(line) == kCrashName)
		return LogLevel::Error;

	const std::string lowered = Lowered(line);

	if (MentionsFailure(lowered) || MentionsError(lowered))
		return LogLevel::Warn;

	return LogLevel::Info;
}

const char* LogText::LevelName(LogLevel level)
{
	const int index = static_cast<int>(level);

	return index < static_cast<int>(LogLevel::Count) ? kLevelNames[index] : kLevelNames[1];
}

const char* LogText::FileTag(LogLevel level)
{
	const int index = static_cast<int>(level);

	return index < static_cast<int>(LogLevel::Count) ? kFileTags[index] : kFileTags[1];
}
