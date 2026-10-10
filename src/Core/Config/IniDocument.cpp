#include "Core/Config/IniDocument.h"

#include <cctype>
#include <cstdint>
#include <cstring>

namespace {

constexpr const char* kBlanks = " \t";
constexpr const char* kNewLine = "\r\n";
constexpr char kComment = ';';
constexpr char kEquals = '=';
constexpr char kOpen = '[';
constexpr char kClose = ']';
constexpr int kDecimal = 10;
constexpr int kHexadecimal = 16;

std::string Trimmed(const std::string& text)
{
	const size_t first = text.find_first_not_of(kBlanks);

	if (first == std::string::npos)
		return std::string();

	return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
}

bool Same(const std::string& left, const std::string& right)
{
	return _stricmp(left.c_str(), right.c_str()) == 0;
}

bool IsHeader(const std::string& trimmed)
{
	return !trimmed.empty() && trimmed[0] == kOpen;
}

std::string HeaderName(const std::string& trimmed)
{
	const size_t close = trimmed.find(kClose);
	const size_t end = close == std::string::npos ? trimmed.size() : close;

	return Trimmed(trimmed.substr(1, end - 1));
}

bool IsEntry(const std::string& trimmed)
{
	return !trimmed.empty() && trimmed[0] != kComment && !IsHeader(trimmed);
}

std::string Unquoted(const std::string& value)
{
	const bool quoted = value.size() >= 2 && (value.front() == '"' || value.front() == '\'') &&
		value.back() == value.front();

	return quoted ? value.substr(1, value.size() - 2) : value;
}

int DigitValue(char letter)
{
	if (isdigit(static_cast<unsigned char>(letter)))
		return letter - '0';

	const int lower = tolower(static_cast<unsigned char>(letter));

	return lower >= 'a' && lower <= 'f' ? lower - 'a' + kDecimal : kHexadecimal;
}

int ParsedInt(const std::string& text)
{
	size_t at = 0;
	const bool negative = at < text.size() && text[at] == '-';

	if (at < text.size() && (text[at] == '-' || text[at] == '+'))
		++at;

	int base = kDecimal;

	if (at + 1 < text.size() && text[at] == '0' && (text[at + 1] == 'x' || text[at + 1] == 'X'))
	{
		base = kHexadecimal;
		at += 2;
	}

	uint32_t value = 0;

	for (; at < text.size() && DigitValue(text[at]) < base; ++at)
		value = value * static_cast<uint32_t>(base) + static_cast<uint32_t>(DigitValue(text[at]));

	return static_cast<int>(negative ? 0u - value : value);
}

}

IniDocument IniDocument::Parse(const std::string& text)
{
	IniDocument document;

	for (size_t begin = 0; begin < text.size();)
	{
		const size_t feed = text.find('\n', begin);
		const size_t end = feed == std::string::npos ? text.size() : feed;
		const bool carriage = end > begin && text[end - 1] == '\r';

		Line line;
		line.text = text.substr(begin, end - begin - (carriage ? 1 : 0));
		line.ending = feed == std::string::npos ? "" : (carriage ? kNewLine : "\n");
		document.m_lines.push_back(line);

		begin = end + 1;
	}

	return document;
}

bool IniDocument::FindSection(const std::string& section, Span& out) const
{
	const std::string wanted = Trimmed(section);

	for (size_t at = 0; at < m_lines.size(); ++at)
	{
		const std::string trimmed = Trimmed(m_lines[at].text);

		if (!IsHeader(trimmed) || !Same(HeaderName(trimmed), wanted))
			continue;

		size_t end = at + 1;

		while (end < m_lines.size() && !IsHeader(Trimmed(m_lines[end].text)))
			++end;

		out = { at, end };
		return true;
	}

	return false;
}

bool IniDocument::FindEntry(const Span& span, const std::string& key, size_t& out) const
{
	const std::string wanted = Trimmed(key);

	for (size_t at = span.header + 1; at < span.end; ++at)
	{
		const std::string& text = m_lines[at].text;
		const size_t equals = text.find(kEquals);

		if (equals == std::string::npos || !IsEntry(Trimmed(text)) || !Same(Trimmed(text.substr(0, equals)), wanted))
			continue;

		out = at;
		return true;
	}

	return false;
}

bool IniDocument::Find(const std::string& section, const std::string& key, std::string& value) const
{
	Span span = {};
	size_t at = 0;

	if (!FindSection(section, span) || !FindEntry(span, key, at))
		return false;

	const std::string& text = m_lines[at].text;
	value = Unquoted(Trimmed(text.substr(text.find(kEquals) + 1)));
	return true;
}

int IniDocument::Int(const std::string& section, const std::string& key, int fallback) const
{
	std::string value;

	return Find(section, key, value) ? ParsedInt(value) : fallback;
}

std::vector<std::string> IniDocument::Section(const std::string& section) const
{
	std::vector<std::string> out;
	Span span = {};

	if (!FindSection(section, span))
		return out;

	for (size_t at = span.header + 1; at < span.end; ++at)
	{
		const std::string trimmed = Trimmed(m_lines[at].text);

		if (!IsEntry(trimmed))
			continue;

		const size_t equals = trimmed.find(kEquals);

		if (equals == std::string::npos)
		{
			out.push_back(trimmed);
			continue;
		}

		out.push_back(Trimmed(trimmed.substr(0, equals)) + kEquals + Trimmed(trimmed.substr(equals + 1)));
	}

	return out;
}

void IniDocument::Insert(size_t at, const std::string& text)
{
	if (at > 0 && m_lines[at - 1].ending.empty())
		m_lines[at - 1].ending = kNewLine;

	m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(at), Line{ text, kNewLine });
}

void IniDocument::Set(const std::string& section, const std::string& key, const std::string& value)
{
	Span span = {};

	if (!FindSection(section, span))
	{
		Insert(m_lines.size(), std::string(1, kOpen) + section + kClose);
		Insert(m_lines.size(), key + kEquals + value);
		return;
	}

	size_t at = 0;

	if (FindEntry(span, key, at))
	{
		std::string& text = m_lines[at].text;
		const size_t last = text.find_last_not_of(kBlanks);
		const std::string tail = last == std::string::npos ? std::string() : text.substr(last + 1);

		text = text.substr(0, text.find(kEquals) + 1) + value + tail;
		return;
	}

	size_t after = span.header + 1;

	for (size_t line = span.header + 1; line < span.end; ++line)
	{
		if (IsEntry(Trimmed(m_lines[line].text)))
			after = line + 1;
	}

	Insert(after, key + kEquals + value);
}

void IniDocument::Remove(const std::string& section, const std::string& key)
{
	Span span = {};
	size_t at = 0;

	if (!FindSection(section, span) || !FindEntry(span, key, at))
		return;

	m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(at));
}

void IniDocument::RemoveSection(const std::string& section)
{
	Span span = {};

	if (!FindSection(section, span))
		return;

	for (size_t at = span.end; at-- > span.header + 1;)
	{
		if (IsEntry(Trimmed(m_lines[at].text)))
			m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(at));
	}

	m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(span.header));
}

std::string IniDocument::Text() const
{
	std::string text;

	for (const Line& line : m_lines)
		text += line.text + line.ending;

	return text;
}
