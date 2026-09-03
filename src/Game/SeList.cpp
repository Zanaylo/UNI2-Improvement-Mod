#include "Game/SeList.h"

#include "Core/TextEncoding.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kAppend = "Path.append(";
constexpr const char* kFile = "file=";
constexpr const char* kPath = "path=";
constexpr const char* kEnd = "};";
constexpr int kMinNoteRunes = 3;

constexpr const char* kNoise[] = {
	"\xe3\x80\x8c", "\xe3\x80\x8d", "\xe3\x80\x8e", "\xe3\x80\x8f",
	"\xef\xbc\x88", "\xef\xbc\x89", "\xe3\x80\x80", "\xef\xbc\x81",
	"\xef\xbc\x9f", "\xe3\x80\x82", "\xe3\x80\x81", "\xe2\x80\xa6",
	"\xe3\x80\x9c", "\xef\xbd\x9e", "\xe2\x98\x85", "\xe2\x80\xbb",
	" ", "\t", "\r", "\n", "(", ")", "!", "?", ".", ",", "~",
};

bool IsUtf8(const std::vector<uint8_t>& bytes)
{
	size_t at = 0;
	bool multibyte = false;

	while (at < bytes.size())
	{
		const uint8_t lead = bytes[at];

		if (lead < 0x80)
		{
			++at;
			continue;
		}

		int extra = 0;

		if ((lead & 0xe0) == 0xc0)
			extra = 1;
		else if ((lead & 0xf0) == 0xe0)
			extra = 2;
		else if ((lead & 0xf8) == 0xf0)
			extra = 3;
		else
			return false;

		if (at + extra >= bytes.size())
			return false;

		for (int i = 1; i <= extra; ++i)
		{
			if ((bytes[at + i] & 0xc0) != 0x80)
				return false;
		}

		multibyte = true;
		at += extra + 1;
	}

	return multibyte;
}

std::string Quoted(const std::string& line, size_t from)
{
	const size_t start = line.find('"', from);

	if (start == std::string::npos)
		return std::string();

	const size_t end = line.find('"', start + 1);

	return end == std::string::npos ? std::string() : line.substr(start + 1, end - start - 1);
}

std::string Variable(const std::string& line, size_t at)
{
	size_t end = at;

	while (end < line.size() && (line[end] == ' ' || line[end] == '\t'))
		++end;

	const size_t start = end;

	while (end < line.size() && (isalnum(static_cast<unsigned char>(line[end])) != 0 ||
		line[end] == '_'))
	{
		++end;
	}

	if (end == start)
		return std::string();

	while (end < line.size() && (line[end] == ' ' || line[end] == '\t'))
		++end;

	return end < line.size() && line[end] == '+' ? line.substr(start, end - start) : std::string();
}

std::string Note(const std::string& line)
{
	const size_t end = line.find(kEnd);

	if (end == std::string::npos)
		return std::string();

	const size_t comment = line.find("//", end);

	if (comment == std::string::npos)
		return std::string();

	std::string text = line.substr(comment + 2);

	while (!text.empty() && (text.back() == '\r' || text.back() == ' ' || text.back() == '\t'))
		text.pop_back();

	size_t head = 0;

	while (head < text.size() && (text[head] == ' ' || text[head] == '\t'))
		++head;

	return text.substr(head);
}

int Path(const std::string& line)
{
	const size_t at = line.find(kPath);

	if (at == std::string::npos)
		return -1;

	return atoi(line.c_str() + at + strlen(kPath));
}

std::string Normalise(const std::string& path)
{
	std::string out = path;

	for (char& c : out)
	{
		if (c == '/')
			c = '\\';
	}

	while (out.compare(0, 2, ".\\") == 0)
		out.erase(0, 2);

	while (!out.empty() && out.back() == '\\')
		out.pop_back();

	return out;
}

int Runes(const std::string& text)
{
	int count = 0;

	for (char c : text)
	{
		if ((static_cast<unsigned char>(c) & 0xc0) != 0x80)
			++count;
	}

	return count;
}

}

std::string SeList::Text(const std::vector<uint8_t>& bytes)
{
	if (bytes.empty())
		return std::string();

	size_t at = 0;

	if (bytes.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf)
		at = 3;

	const std::vector<uint8_t> body(bytes.begin() + at, bytes.end());

	if (IsUtf8(body))
		return std::string(reinterpret_cast<const char*>(body.data()), body.size());

	std::string out;

	if (!TextEncoding::ShiftJisToUtf8(reinterpret_cast<const char*>(body.data()), body.size(), out))
		return std::string(reinterpret_cast<const char*>(body.data()), body.size());

	return out;
}

void SeList::Parse(const std::string& text, File& out)
{
	out.paths.clear();
	out.rows.clear();

	size_t at = 0;

	while (at < text.size())
	{
		size_t end = text.find('\n', at);

		if (end == std::string::npos)
			end = text.size();

		const std::string line = text.substr(at, end - at);
		at = end + 1;

		const size_t appended = line.find(kAppend);

		if (appended != std::string::npos)
		{
			const std::string folder = Quoted(line, appended);

			if (!folder.empty())
				out.paths.push_back(Normalise(folder));

			continue;
		}

		const size_t named = line.find(kFile);

		if (named == std::string::npos)
			continue;

		Row row;
		row.stem = Quoted(line, named);

		if (row.stem.empty())
			continue;

		row.variable = Variable(line, named + strlen(kFile));
		row.note = Note(line);
		row.path = Path(line);

		out.rows.push_back(row);
	}
}

std::string SeList::NoteKey(const std::string& note)
{
	std::string out = note;

	for (const char* noise : kNoise)
	{
		const size_t length = strlen(noise);

		for (size_t at = out.find(noise); at != std::string::npos; at = out.find(noise, at))
			out.erase(at, length);
	}

	return Runes(out) >= kMinNoteRunes ? out : std::string();
}

std::string SeList::Number(const std::string& stem)
{
	std::string out;

	for (size_t at = 0; at + 1 < stem.size(); ++at)
	{
		if (stem[at] != '_' || isdigit(static_cast<unsigned char>(stem[at + 1])) == 0)
			continue;

		size_t end = at + 1;

		while (end < stem.size() && isdigit(static_cast<unsigned char>(stem[end])) != 0)
			++end;

		out = stem.substr(at + 1, end - at - 1);
		at = end - 1;
	}

	return out;
}
