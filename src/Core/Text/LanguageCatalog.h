#pragma once

#include <cstddef>
#include <string_view>
#include <unordered_map>

class LanguageCatalog
{
public:
	struct Entry
	{
		const char* english;
		const char* text;
	};

	struct Table
	{
		const Entry* entries;
		size_t count;
	};

	void Add(const Table& table);

	const char* Find(std::string_view english) const;
	size_t Size() const { return m_texts.size(); }

private:
	std::unordered_map<std::string_view, const char*> m_texts;
};
