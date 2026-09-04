#include "Game/BgListOverride.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/DataArchive.h"
#include "Game/StageArchive.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr const char* kList = "BgList.txt";
constexpr const char* kNames = "BgList_str.txt";

std::string Root()
{
	return GetModRootPath("Mods\\bg");
}

std::string Path(const char* leaf)
{
	return Root() + "\\" + leaf;
}

bool Read(const char* leaf, std::string& out)
{
	std::vector<uint8_t> blob;

	if (ReadWholeFile(Path(leaf), blob) && !blob.empty())
	{
		out.assign(blob.begin(), blob.end());
		return true;
	}

	if (!DataArchive::Read("bg", leaf, blob) || blob.empty())
		return false;

	out.assign(blob.begin(), blob.end());
	return true;
}

bool Write(const char* leaf, const std::string& text)
{
	CreateDirectoryTree(Root());

	FILE* handle = nullptr;

	if (fopen_s(&handle, Path(leaf).c_str(), "wb") != 0 || handle == nullptr)
		return false;

	const size_t written = fwrite(text.data(), 1, text.size(), handle);
	fclose(handle);

	return written == text.size();
}

std::string Tag(int number)
{
	char tag[16] = {};
	sprintf_s(tag, "Bg_%03d", number);

	return tag;
}

size_t TableEnd(const std::string& list)
{
	const size_t table = list.find("<-");
	const size_t open = table == std::string::npos ? table : list.find('{', table);
	const size_t end = open == std::string::npos
		? std::string::npos : StageArchive::MatchPair(list, open);

	return end == std::string::npos ? end : end - 1;
}

void DropEntry(std::string& list, int number)
{
	size_t start = list.find(Tag(number));
	const size_t open = start == std::string::npos ? start : list.find('{', start);

	if (open == std::string::npos)
		return;

	size_t end = StageArchive::MatchPair(list, open);

	if (end == std::string::npos)
		return;

	while (start > 0 && (list[start - 1] == '\t' || list[start - 1] == ' '))
		--start;

	while (end < list.size() && (list[end] == '\r' || list[end] == '\n'))
		++end;

	list.erase(start, end - start);
}

bool ListedNumbers(const std::string& list, std::vector<int>& out, size_t& first, size_t& last)
{
	const size_t open = list.find("BgSelectList");

	first = open == std::string::npos ? open : list.find('[', open);
	last = first == std::string::npos ? first : list.find(']', first);

	if (last == std::string::npos)
		return false;

	for (size_t at = first + 1; at < last; ++at)
	{
		if (isdigit(static_cast<unsigned char>(list[at])) == 0)
			continue;

		out.push_back(atoi(list.c_str() + at));

		while (at < last && isdigit(static_cast<unsigned char>(list[at])) != 0)
			++at;
	}

	return true;
}

void SetListed(std::string& list, int number, bool listed)
{
	std::vector<int> numbers;
	size_t first = 0;
	size_t last = 0;

	if (!ListedNumbers(list, numbers, first, last))
		return;

	numbers.erase(std::remove(numbers.begin(), numbers.end(), number), numbers.end());

	if (listed)
		numbers.push_back(number);

	std::string rebuilt = " ";

	for (size_t i = 0; i < numbers.size(); ++i)
	{
		char text[16] = {};
		sprintf_s(text, "%d", numbers[i]);

		rebuilt += text;
		rebuilt += i + 1 < numbers.size() ? ", " : " ";
	}

	list.replace(first + 1, last - first - 1, rebuilt);
}

bool LineOf(const std::string& names, int number, size_t& start, size_t& end)
{
	char wanted[8] = {};
	sprintf_s(wanted, "%03d", number);

	for (size_t at = 0; at != std::string::npos; at = names.find('\n', at + 1))
	{
		const size_t line = at == 0 ? 0 : at + 1;

		if (names.compare(line, 3, wanted) != 0)
			continue;

		size_t after = line + 3;

		while (after < names.size() && (names[after] == ' ' || names[after] == '\t'))
			++after;

		if (after >= names.size() || names[after] != '=')
			continue;

		start = line;
		end = names.find('\n', line);
		end = end == std::string::npos ? names.size() : end + 1;
		return true;
	}

	return false;
}

void SetName(std::string& names, int number, const std::string& shiftJisName)
{
	char line[256] = {};
	sprintf_s(line, "%03d = %s\r\n", number, shiftJisName.c_str());

	size_t start = 0;
	size_t end = 0;

	if (LineOf(names, number, start, end))
	{
		names.replace(start, end - start, line);
		return;
	}

	if (!names.empty() && names.back() != '\n')
		names += "\r\n";

	names += line;
}

void ClearName(std::string& names, int number)
{
	size_t start = 0;
	size_t end = 0;

	if (LineOf(names, number, start, end))
		names.erase(start, end - start);
}

bool NameIs(const std::string& names, int number, const std::string& wanted)
{
	size_t start = 0;
	size_t end = 0;

	if (!LineOf(names, number, start, end))
		return false;

	std::string line = names.substr(start, end - start);

	while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
		line.pop_back();

	const size_t equals = line.find('=');
	std::string value = equals == std::string::npos ? std::string() : line.substr(equals + 1);

	while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
		value.erase(0, 1);

	return value == wanted;
}

bool HasName(const std::string& names, int number)
{
	size_t start = 0;
	size_t end = 0;

	return LineOf(names, number, start, end);
}

std::string NameIn(const std::string& list, int number)
{
	std::string block;
	std::string name;

	if (!StageArchive::Block(list, Tag(number), block) ||
		!StageArchive::Field(block, "Name", name))
	{
		return std::string();
	}

	if (!name.empty() && name.front() == '"')
		name = name.substr(1, name.size() - (name.back() == '"' ? 2 : 1));

	return name;
}

bool g_restart = false;

bool Commit(const std::string& list, const std::string& names)
{
	if (!Write(kList, list) || !Write(kNames, names))
		return false;

	g_restart = true;
	return true;
}

}

bool BgListOverride::Add(int number, const std::string& entry, const std::string& shiftJisName)
{
	std::string list;
	std::string names;

	if (!Read(kList, list) || !Read(kNames, names))
		return false;

	DropEntry(list, number);
	SetListed(list, number, true);

	const size_t close = TableEnd(list);

	if (close == std::string::npos)
		return false;

	list.insert(close, entry + "\r\n");
	SetName(names, number, shiftJisName);

	return Commit(list, names);
}

bool BgListOverride::Drop(int number)
{
	std::string list;
	std::string names;

	if (!Read(kList, list) || !Read(kNames, names))
		return false;

	DropEntry(list, number);
	SetListed(list, number, false);
	ClearName(names, number);

	return Commit(list, names);
}

bool BgListOverride::Show(int number, bool shown, const std::string& shiftJisName)
{
	std::string list;
	std::string names;

	if (!Read(kList, list) || !Read(kNames, names))
		return false;

	SetListed(list, number, shown);

	if (shown && !HasName(names, number))
	{
		const std::string own = shiftJisName.empty() ? NameIn(list, number) : shiftJisName;

		if (!own.empty())
			SetName(names, number, own);
	}

	LOG("BgListOverride: stage %d %s the picker list", number, shown ? "joins" : "leaves");

	return Commit(list, names);
}

bool BgListOverride::IsListed(int number)
{
	std::string list;

	if (!Read(kList, list))
		return false;

	std::vector<int> numbers;
	size_t first = 0;
	size_t last = 0;

	if (!ListedNumbers(list, numbers, first, last))
		return false;

	return std::find(numbers.begin(), numbers.end(), number) != numbers.end();
}

bool BgListOverride::NeedsRestart()
{
	return g_restart;
}

bool BgListOverride::OwnName(int number, std::string& out)
{
	std::string list;

	if (!Read(kList, list))
		return false;

	out = NameIn(list, number);
	return !out.empty();
}

bool BgListOverride::SetNames(const std::vector<std::pair<int, std::string> >& named)
{
	std::string names;

	if (!Read(kNames, names))
		return false;

	bool changed = false;

	for (const std::pair<int, std::string>& entry : named)
	{
		if (entry.second.empty() || NameIs(names, entry.first, entry.second))
			continue;

		SetName(names, entry.first, entry.second);
		changed = true;

		LOG("BgListOverride: stage %d is '%s' in the picker now", entry.first,
			entry.second.c_str());
	}

	if (!changed)
		return true;

	std::string list;

	return Read(kList, list) && Commit(list, names);
}
