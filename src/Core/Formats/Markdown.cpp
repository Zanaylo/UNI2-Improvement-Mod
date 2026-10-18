#include "Core/Formats/Markdown.h"

#include <cctype>
#include <cstring>

namespace {

constexpr const char* kFence = "```";
constexpr const char* kRules[] = { "---", "***", "___" };
constexpr const char* kBulletMarks = "-*+";
constexpr const char* kUrlStops = ")]<>\"";
constexpr const char* kUrlTail = ".,;:!?)";
constexpr const char* kTaskMarks[] = { "[ ] ", "[x] ", "[X] " };
constexpr size_t kTaskMarkBytes = 4;
constexpr int kMostHeadingLevel = 6;
constexpr int kSpacesPerLevel = 2;
constexpr int kTabSpaces = 4;

using Markdown::Block;
using Markdown::Kind;
using Markdown::Piece;
using Markdown::Style;

struct Span
{
	std::string text;
	std::string url;
	Style style;
};

bool IsSpace(char c)
{
	return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool IsWordChar(char c)
{
	return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

std::string Trim(const std::string& text)
{
	size_t first = 0;

	while (first < text.size() && IsSpace(text[first]))
		++first;

	size_t last = text.size();

	while (last > first && IsSpace(text[last - 1]))
		--last;

	return text.substr(first, last - first);
}

bool StartsWith(const std::string& text, const char* prefix)
{
	return text.compare(0, strlen(prefix), prefix) == 0;
}

std::vector<std::string> Lines(const std::string& text)
{
	std::vector<std::string> lines;
	std::string current;

	for (const char c : text)
	{
		if (c == '\n')
		{
			lines.push_back(current);
			current.clear();
			continue;
		}

		if (c != '\r')
			current.push_back(c);
	}

	if (!current.empty())
		lines.push_back(current);

	return lines;
}

int Indentation(const std::string& line)
{
	int spaces = 0;

	for (const char c : line)
	{
		if (c == ' ')
			++spaces;
		else if (c == '\t')
			spaces += kTabSpaces;
		else
			break;
	}

	return spaces;
}

class Tokenizer
{
public:
	explicit Tokenizer(const std::string& line)
		: m_line(line)
	{
	}

	std::vector<Span> Run()
	{
		while (m_at < m_line.size())
		{
			if (!Escape() && !Code() && !NamedLink() && !BareLink() && !Emphasis())
				m_plain.push_back(m_line[m_at++]);
		}

		Flush();
		return m_spans;
	}

private:
	void Flush()
	{
		if (m_plain.empty())
			return;

		m_spans.push_back({ m_plain, std::string(), Style::Normal });
		m_plain.clear();
	}

	void Emit(const std::string& text, Style style, const std::string& url = std::string())
	{
		Flush();
		m_spans.push_back({ text, url, style });
	}

	bool Escape()
	{
		if (m_line[m_at] != '\\' || m_at + 1 >= m_line.size())
			return false;

		m_plain.push_back(m_line[m_at + 1]);
		m_at += 2;
		return true;
	}

	bool Code()
	{
		if (m_line[m_at] != '`')
			return false;

		const size_t close = m_line.find('`', m_at + 1);

		if (close == std::string::npos)
			return false;

		Emit(m_line.substr(m_at + 1, close - m_at - 1), Style::Code);
		m_at = close + 1;
		return true;
	}

	bool NamedLink()
	{
		if (m_line[m_at] != '[')
			return false;

		const size_t closeText = m_line.find(']', m_at + 1);

		if (closeText == std::string::npos || closeText + 1 >= m_line.size() || m_line[closeText + 1] != '(')
			return false;

		const size_t closeUrl = m_line.find(')', closeText + 2);

		if (closeUrl == std::string::npos)
			return false;

		Emit(m_line.substr(m_at + 1, closeText - m_at - 1), Style::Link,
			m_line.substr(closeText + 2, closeUrl - closeText - 2));
		m_at = closeUrl + 1;
		return true;
	}

	bool BareLink()
	{
		if (m_line.compare(m_at, 7, "http://") != 0 && m_line.compare(m_at, 8, "https://") != 0)
			return false;

		size_t end = m_at;

		while (end < m_line.size() && !IsSpace(m_line[end]) && strchr(kUrlStops, m_line[end]) == nullptr)
			++end;

		while (end > m_at && strchr(kUrlTail, m_line[end - 1]) != nullptr)
			--end;

		const std::string url = m_line.substr(m_at, end - m_at);
		Emit(url, Style::Link, url);
		m_at = end;
		return true;
	}

	bool Emphasis()
	{
		return Wrapped("**", Style::Bold) || Wrapped("__", Style::Bold) || Wrapped("~~", Style::Strike) ||
			Wrapped("*", Style::Normal) || Wrapped("_", Style::Normal);
	}

	bool Wrapped(const char* marker, Style style)
	{
		const size_t length = strlen(marker);

		if (m_line.compare(m_at, length, marker) != 0)
			return false;

		if (marker[0] == '_' && m_at > 0 && IsWordChar(m_line[m_at - 1]))
			return false;

		const size_t close = m_line.find(marker, m_at + length);

		if (close == std::string::npos || close == m_at + length)
			return false;

		if (marker[0] == '_' && close + length < m_line.size() && IsWordChar(m_line[close + length]))
			return false;

		Emit(m_line.substr(m_at + length, close - m_at - length), style);
		m_at = close + length;
		return true;
	}

	const std::string& m_line;
	size_t m_at = 0;
	std::string m_plain;
	std::vector<Span> m_spans;
};

void SplitWords(const Span& span, bool& pendingSpace, std::vector<Piece>& out)
{
	std::string word;

	const auto flush = [&]()
	{
		if (word.empty())
			return;

		out.push_back({ word, std::string(), span.style, pendingSpace });
		word.clear();
		pendingSpace = false;
	};

	for (const char c : span.text)
	{
		if (!IsSpace(c))
		{
			word.push_back(c);
			continue;
		}

		flush();
		pendingSpace = true;
	}

	flush();
}

std::vector<Piece> Inline(const std::string& line)
{
	std::vector<Piece> pieces;
	bool pendingSpace = false;

	for (const Span& span : Tokenizer(line).Run())
	{
		if (span.style != Style::Code && span.style != Style::Link)
		{
			SplitWords(span, pendingSpace, pieces);
			continue;
		}

		pieces.push_back({ span.text, span.url, span.style, pendingSpace });
		pendingSpace = false;
	}

	return pieces;
}

bool IsRule(const std::string& trimmed)
{
	for (const char* rule : kRules)
	{
		if (trimmed == rule)
			return true;
	}

	return false;
}

int HeadingLevel(const std::string& trimmed)
{
	int level = 0;

	while (level < static_cast<int>(trimmed.size()) && level < kMostHeadingLevel && trimmed[level] == '#')
		++level;

	if (level == 0 || level >= static_cast<int>(trimmed.size()) || trimmed[level] != ' ')
		return 0;

	return level;
}

size_t NumberedMarkerLength(const std::string& trimmed)
{
	size_t digits = 0;

	while (digits < trimmed.size() && std::isdigit(static_cast<unsigned char>(trimmed[digits])))
		++digits;

	if (digits == 0 || digits + 1 >= trimmed.size())
		return 0;

	if ((trimmed[digits] != '.' && trimmed[digits] != ')') || trimmed[digits + 1] != ' ')
		return 0;

	return digits + 1;
}

bool IsBullet(const std::string& trimmed)
{
	return trimmed.size() > 1 && strchr(kBulletMarks, trimmed[0]) != nullptr && trimmed[1] == ' ';
}

Block ListItem(Kind kind, int level, const std::string& marker, const std::string& text)
{
	Block block;
	block.kind = kind;
	block.level = level;
	block.marker = marker;
	block.pieces = Inline(Trim(text));
	return block;
}

Block Bullet(const std::string& raw, const std::string& trimmed)
{
	const int level = Indentation(raw) / kSpacesPerLevel;
	const std::string text = trimmed.substr(2);

	for (const char* task : kTaskMarks)
	{
		if (StartsWith(text, task))
			return ListItem(Kind::Bullet, level, text.substr(0, kTaskMarkBytes - 1), text.substr(kTaskMarkBytes));
	}

	return ListItem(Kind::Bullet, level, std::string(), text);
}

Block Simple(Kind kind, int level, const std::string& text)
{
	Block block;
	block.kind = kind;
	block.level = level;
	block.pieces = Inline(text);
	return block;
}

Block Verbatim(const std::string& raw)
{
	Block block;
	block.kind = Kind::Code;
	block.pieces.push_back({ raw, std::string(), Style::Code, false });
	return block;
}

Block LineBlock(const std::string& raw, const std::string& trimmed)
{
	if (IsRule(trimmed))
		return Simple(Kind::Rule, 0, std::string());

	const int heading = HeadingLevel(trimmed);

	if (heading > 0)
		return Simple(Kind::Heading, heading, Trim(trimmed.substr(heading + 1)));

	if (trimmed[0] == '>')
		return Simple(Kind::Quote, 0, Trim(trimmed.substr(1)));

	if (IsBullet(trimmed))
		return Bullet(raw, trimmed);

	const size_t numbered = NumberedMarkerLength(trimmed);

	if (numbered > 0)
		return ListItem(Kind::Numbered, Indentation(raw) / kSpacesPerLevel, trimmed.substr(0, numbered),
			trimmed.substr(numbered + 1));

	return Simple(Kind::Paragraph, 0, trimmed);
}

void AddGap(std::vector<Block>& blocks)
{
	if (blocks.empty() || blocks.back().kind == Kind::Gap)
		return;

	Block gap;
	gap.kind = Kind::Gap;
	blocks.push_back(gap);
}

}

std::vector<Block> Markdown::Parse(const std::string& text)
{
	std::vector<Block> blocks;
	bool inCode = false;

	for (const std::string& raw : Lines(text))
	{
		const std::string trimmed = Trim(raw);

		if (StartsWith(trimmed, kFence))
		{
			inCode = !inCode;
			continue;
		}

		if (inCode)
		{
			blocks.push_back(Verbatim(raw));
			continue;
		}

		if (trimmed.empty())
		{
			AddGap(blocks);
			continue;
		}

		blocks.push_back(LineBlock(raw, trimmed));
	}

	while (!blocks.empty() && blocks.back().kind == Kind::Gap)
		blocks.pop_back();

	return blocks;
}
