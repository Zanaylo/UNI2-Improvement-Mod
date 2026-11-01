#include "Core/Text/TextList.h"

#include <cctype>
#include <cstring>

namespace {

constexpr const char* kBlanks = " \t\r\n";

}

std::string TextList::Trimmed(const std::string& text)
{
	const size_t first = text.find_first_not_of(kBlanks);

	if (first == std::string::npos)
		return std::string();

	const size_t last = text.find_last_not_of(kBlanks);
	return text.substr(first, last - first + 1);
}

std::vector<std::string> TextList::Split(const std::string& text, const char* separators)
{
	std::vector<std::string> items;
	size_t begin = 0;

	while (begin <= text.size())
	{
		const size_t end = text.find_first_of(separators, begin);
		const size_t stop = end == std::string::npos ? text.size() : end;
		const std::string item = Trimmed(text.substr(begin, stop - begin));

		if (!item.empty())
			items.push_back(item);

		if (end == std::string::npos)
			break;

		begin = end + 1;
	}

	return items;
}

std::string TextList::Join(const std::vector<std::string>& items, char separator)
{
	std::string joined;

	for (const std::string& item : items)
	{
		if (!joined.empty())
			joined.push_back(separator);

		joined += item;
	}

	return joined;
}

bool TextList::SameNoCase(const std::string& a, const std::string& b)
{
	return a.size() == b.size() && _stricmp(a.c_str(), b.c_str()) == 0;
}
