#include "Core/Log/LogText.h"

#include <cstring>

namespace {

constexpr const char* kGeneral = "General";
constexpr const char* kCrashName = "CRASH";
constexpr size_t kCrashLength = 5;
constexpr const char* kErrorWord = "error";
constexpr size_t kErrorLength = 5;
constexpr int kMostSpaces = 1;

const char* const kWarnPhrases[] = { "could not", "cannot", "can't", "failed", "failure", "unable to" };
const char* const kHarmlessBeforeError[] = { "no ", "0 " };

const char* const kLevelNames[] = { "Debug", "Info", "Warning", "Error" };
const char* const kFileTags[] = { "[debug] ", "", "[warn] ", "[error] " };

bool IsAlpha(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool IsDigit(char c)
{
	return c >= '0' && c <= '9';
}

bool IsNameLetter(char c)
{
	return IsAlpha(c) || IsDigit(c) || c == '_' || c == '-' || c == '.';
}

char LowerAscii(char c)
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

bool FindCategorySpan(const char* line, size_t& outLength)
{
	if (line == nullptr || !IsAlpha(line[0]))
		return false;

	int spaces = 0;
	size_t scope = 0;
	bool hasScope = false;

	for (size_t at = 0; at <= LogText::kLongestCategory && line[at] != '\0'; ++at)
	{
		const char letter = line[at];

		if (letter == ':' && (line[at + 1] == ' ' || line[at + 1] == '\0'))
		{
			outLength = hasScope ? scope : at;
			return true;
		}

		if (letter == ':' && line[at + 1] == ':')
		{
			if (!hasScope)
			{
				scope = at;
				hasScope = true;
			}
			++at;
			continue;
		}

		if (letter == ' ' && ++spaces <= kMostSpaces)
			continue;

		if (!IsNameLetter(letter))
			return false;
	}

	return false;
}

bool MatchLower(const char* text, const char* needle)
{
	for (size_t i = 0; needle[i] != '\0'; ++i)
	{
		if (text[i] == '\0' || LowerAscii(text[i]) != needle[i])
			return false;
	}

	return true;
}

bool ContainsLower(const char* text, const char* needle)
{
	const char first = needle[0];

	for (const char* p = text; *p != '\0'; ++p)
	{
		if (LowerAscii(*p) == first && MatchLower(p + 1, needle + 1))
			return true;
	}

	return false;
}

bool FollowsHarmless(const char* line, size_t at)
{
	for (const char* harmless : kHarmlessBeforeError)
	{
		const size_t length = strlen(harmless);

		if (at >= length && MatchLower(line + at - length, harmless))
			return true;
	}

	return false;
}

bool MentionsError(const char* line)
{
	for (size_t at = 0; line[at] != '\0'; ++at)
	{
		if (LowerAscii(line[at]) != 'e' || !MatchLower(line + at + 1, kErrorWord + 1))
			continue;

		const bool startsWord = at == 0 || !IsNameLetter(line[at - 1]);
		const char after = line[at + kErrorLength];
		const bool endsWord = after == '\0' || !IsAlpha(after);

		if (startsWord && endsWord && !FollowsHarmless(line, at))
			return true;
	}

	return false;
}

bool MentionsFailure(const char* line)
{
	for (const char* phrase : kWarnPhrases)
	{
		if (ContainsLower(line, phrase))
			return true;
	}

	return false;
}

}

std::string LogText::Category(const char* line)
{
	size_t length = 0;

	if (!FindCategorySpan(line, length))
		return kGeneral;

	return std::string(line, length);
}

LogLevel LogText::Guess(const char* line)
{
	if (line == nullptr)
		return LogLevel::Info;

	size_t categoryLength = 0;

	if (FindCategorySpan(line, categoryLength) && categoryLength == kCrashLength &&
		memcmp(line, kCrashName, kCrashLength) == 0)
	{
		return LogLevel::Error;
	}

	if (MentionsFailure(line) || MentionsError(line))
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
