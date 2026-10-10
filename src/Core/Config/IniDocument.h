#pragma once

#include <cstddef>
#include <string>
#include <vector>

class IniDocument
{
public:
	static IniDocument Parse(const std::string& text);

	bool Find(const std::string& section, const std::string& key, std::string& value) const;
	int Int(const std::string& section, const std::string& key, int fallback) const;
	std::vector<std::string> Section(const std::string& section) const;

	void Set(const std::string& section, const std::string& key, const std::string& value);
	void Remove(const std::string& section, const std::string& key);
	void RemoveSection(const std::string& section);

	std::string Text() const;

private:
	struct Line
	{
		std::string text;
		std::string ending;
	};

	struct Span
	{
		size_t header;
		size_t end;
	};

	bool FindSection(const std::string& section, Span& out) const;
	bool FindEntry(const Span& span, const std::string& key, size_t& out) const;
	void Insert(size_t at, const std::string& text);

	std::vector<Line> m_lines;
};
