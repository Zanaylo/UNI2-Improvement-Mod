#include "Game/BgListOverride.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/DataArchive.h"
#include "Game/StageArchive.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

bool Write(const char* leaf, const std::string& text, bool& changed)
{
	std::vector<uint8_t> was;

	if (ReadWholeFile(Path(leaf), was) && was.size() == text.size()
		&& memcmp(was.data(), text.data(), text.size()) == 0)
	{
		return true;
	}

	CreateDirectoryTree(Root());

	FILE* handle = nullptr;

	if (fopen_s(&handle, Path(leaf).c_str(), "wb") != 0 || handle == nullptr)
		return false;

	const size_t written = fwrite(text.data(), 1, text.size(), handle);
	fclose(handle);

	changed = changed || written == text.size();

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

bool Span(const std::string& list, int number, size_t& start, size_t& end)
{
	start = list.find(Tag(number));
	const size_t open = start == std::string::npos ? start : list.find('{', start);

	if (open == std::string::npos)
		return false;

	end = StageArchive::MatchPair(list, open);

	if (end == std::string::npos)
		return false;

	while (start > 0 && (list[start - 1] == '\t' || list[start - 1] == ' '))
		--start;

	while (end < list.size() && (list[end] == '\r' || list[end] == '\n'))
		++end;

	return true;
}

void DropEntry(std::string& list, int number)
{
	size_t start = 0;
	size_t end = 0;

	if (Span(list, number, start, end))
		list.erase(start, end - start);
}

void EntryNumbers(const std::string& list, std::vector<int>& out)
{
	const std::string mark = "Bg_";

	for (size_t at = list.find(mark); at != std::string::npos; at = list.find(mark, at + 1))
	{
		const size_t digits = at + mark.size();

		if (digits + 3 > list.size())
			return;

		if (isdigit(static_cast<unsigned char>(list[digits])) == 0 ||
			isdigit(static_cast<unsigned char>(list[digits + 1])) == 0 ||
			isdigit(static_cast<unsigned char>(list[digits + 2])) == 0)
		{
			continue;
		}

		size_t after = digits + 3;

		while (after < list.size() && (list[after] == ' ' || list[after] == '\t'))
			++after;

		if (after >= list.size() || list[after] != '=')
			continue;

		out.push_back(atoi(list.substr(digits, 3).c_str()));
	}
}

bool Original(const char* leaf, std::string& out)
{
	std::vector<uint8_t> blob;

	if (!DataArchive::Read("bg", leaf, blob) || blob.empty())
		return false;

	out.assign(blob.begin(), blob.end());
	return true;
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

void Restock(std::string& list, std::string& names, const std::vector<int>& owned)
{
	std::string original;
	std::string originalNames;

	if (!Original(kList, original) || !Original(kNames, originalNames))
		return;

	const size_t table = TableEnd(list);

	if (table == std::string::npos)
		return;

	std::vector<int> numbers;
	EntryNumbers(original, numbers);

	std::vector<int> lost;
	std::string added;

	for (int number : numbers)
	{
		if (std::find(owned.begin(), owned.end(), number) != owned.end())
			continue;

		size_t start = 0;
		size_t end = 0;

		if (Span(list, number, start, end) || !Span(original, number, start, end))
			continue;

		added += original.substr(start, end - start);
		lost.push_back(number);
	}

	if (lost.empty())
		return;

	list.insert(table, added);

	std::vector<int> listed;
	size_t first = 0;
	size_t last = 0;
	ListedNumbers(original, listed, first, last);

	for (int number : lost)
	{
		if (std::find(listed.begin(), listed.end(), number) != listed.end())
			SetListed(list, number, true);

		size_t start = 0;
		size_t end = 0;

		if (!HasName(names, number) && LineOf(originalNames, number, start, end))
		{
			if (!names.empty() && names.back() != '\n')
				names += "\r\n";

			names += originalNames.substr(start, end - start);
		}

		LOG("BgListOverride: stage %d had gone from the table and is put back", number);
	}
}

bool g_restart = false;

bool Commit(const std::string& list, const std::string& names)
{
	bool changed = false;

	if (!Write(kList, list, changed) || !Write(kNames, names, changed))
		return false;

	g_restart = g_restart || changed;
	return true;
}

}

bool BgListOverride::Sync(const std::vector<Slotted>& ours, const std::vector<int>& owned)
{
	std::string list;
	std::string names;

	if (!Read(kList, list) || !Read(kNames, names))
		return false;

	Restock(list, names, owned);

	for (int number : owned)
	{
		DropEntry(list, number);
		ClearName(names, number);
	}

	std::vector<int> order;
	size_t open = 0;
	size_t close = 0;

	if (!ListedNumbers(list, order, open, close))
		return false;

	order.erase(std::remove_if(order.begin(), order.end(), [&owned](int number)
		{ return std::find(owned.begin(), owned.end(), number) != owned.end(); }), order.end());

	const size_t table = TableEnd(list);

	if (table == std::string::npos)
		return false;

	std::string added;

	for (const Slotted& one : ours)
	{
		added += one.entry;
		added += "\r\n";

		SetName(names, one.number, one.shiftJisName);
		order.push_back(one.number);
	}

	list.insert(table, added);

	std::vector<int> moved;

	if (!ListedNumbers(list, moved, open, close))
		return false;

	std::string rebuilt = " ";

	for (size_t i = 0; i < order.size(); ++i)
	{
		char text[16] = {};
		sprintf_s(text, "%d", order[i]);

		rebuilt += text;
		rebuilt += i + 1 < order.size() ? ", " : " ";
	}

	list.replace(open + 1, close - open - 1, rebuilt);

	LOG("BgListOverride: the picker list carries %d of ours now",
		static_cast<int>(ours.size()));

	return Commit(list, names);
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

bool BgListOverride::Restore(int number)
{
	std::string list;
	std::string names;
	std::string ownList;
	std::string ownNames;

	if (!Read(kList, list) || !Read(kNames, names) || !Original(kList, ownList)
		|| !Original(kNames, ownNames))
	{
		return false;
	}

	size_t start = 0;
	size_t end = 0;

	if (!Span(ownList, number, start, end))
		return false;

	const std::string entry = ownList.substr(start, end - start);

	DropEntry(list, number);

	const size_t close = TableEnd(list);

	if (close == std::string::npos)
		return false;

	list.insert(close, entry);

	std::vector<int> ownOrder;
	size_t first = 0;
	size_t last = 0;

	ListedNumbers(ownList, ownOrder, first, last);
	SetListed(list, number, std::find(ownOrder.begin(), ownOrder.end(), number) != ownOrder.end());

	ClearName(names, number);

	if (LineOf(ownNames, number, start, end))
	{
		if (!names.empty() && names.back() != '\n')
			names += "\r\n";

		names += ownNames.substr(start, end - start);
	}

	LOG("BgListOverride: stage %d is the game's own again", number);

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

bool BgListOverride::Body(int number, std::string& out)
{
	std::string list;

	if (!Read(kList, list))
		return false;

	return StageArchive::Block(list, Tag(number), out);
}

bool BgListOverride::SelectOrder(std::vector<int>& out)
{
	out.clear();

	std::string list;

	if (!Read(kList, list))
		return false;

	size_t first = 0;
	size_t last = 0;

	return ListedNumbers(list, out, first, last);
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
