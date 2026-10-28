#include "Game/Stages/BgTable.h"

#include "Game/Stages/PickerOrder.h"
#include "Game/Stages/SquirrelText.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kMark = "Bg_";
constexpr const char* kTable = "<-";
constexpr const char* kListKey = "BgSelectList";
constexpr const char* kLineEnd = "\r\n";
constexpr const char* kBlanks = " \t";
constexpr size_t kDigits = 3;

bool Contains(const std::vector<int>& numbers, int number)
{
	return std::find(numbers.begin(), numbers.end(), number) != numbers.end();
}

bool DigitsAt(const std::string& text, size_t at)
{
	if (at + kDigits > text.size())
		return false;

	for (size_t i = 0; i < kDigits; ++i)
	{
		if (isdigit(static_cast<unsigned char>(text[at + i])) == 0)
			return false;
	}

	return true;
}

bool AssignedAfter(const std::string& text, size_t at)
{
	const size_t equals = SquirrelText::Skip(text, at, kBlanks);

	return equals < text.size() && text[equals] == '=';
}

std::string Padded(int number)
{
	char digits[16] = {};
	sprintf_s(digits, "%03d", number);

	return digits;
}

size_t TagOf(const std::string& list, int number)
{
	const std::string tag = kMark + Padded(number);

	for (size_t at = list.find(tag); at != std::string::npos; at = list.find(tag, at + 1))
	{
		if (AssignedAfter(list, at + tag.size()))
			return at;
	}

	return std::string::npos;
}

bool BlockSpan(const std::string& list, int number, size_t& start, size_t& close)
{
	start = TagOf(list, number);
	const size_t open = start == std::string::npos ? start : list.find('{', start);

	if (open == std::string::npos)
		return false;

	close = SquirrelText::MatchPair(list, open);

	if (close == std::string::npos)
		return false;

	while (start > 0 && (list[start - 1] == '\t' || list[start - 1] == ' '))
		--start;

	return true;
}

size_t TableEnd(const std::string& list)
{
	const size_t table = list.find(kTable);
	const size_t open = table == std::string::npos ? table : list.find('{', table);
	const size_t end = open == std::string::npos ? open : SquirrelText::MatchPair(list, open);

	return end == std::string::npos ? end : end - 1;
}

bool ListSpan(const std::string& list, size_t& open, size_t& close)
{
	const size_t key = list.find(kListKey);

	open = key == std::string::npos ? key : list.find('[', key);
	close = open == std::string::npos ? open : SquirrelText::MatchPair(list, open);

	return close != std::string::npos;
}

std::string Unlined(const std::string& text)
{
	std::string unlined = text;

	while (!unlined.empty() && (unlined.back() == '\r' || unlined.back() == '\n'))
		unlined.pop_back();

	return unlined;
}

std::string Lined(const std::string& text)
{
	return Unlined(text) + kLineEnd;
}

bool LineSpan(const std::string& names, int number, size_t& start, size_t& end)
{
	const std::string wanted = Padded(number);

	for (size_t line = 0; line < names.size();)
	{
		const size_t next = names.find('\n', line);
		const size_t lineEnd = next == std::string::npos ? names.size() : next + 1;

		if (names.compare(line, kDigits, wanted) == 0 && AssignedAfter(names, line + kDigits))
		{
			start = line;
			end = lineEnd;
			return true;
		}

		line = lineEnd;
	}

	return false;
}

std::string Value(const std::string& list, int number, const char* key)
{
	std::string entry;
	std::string value;

	if (!BgTable::Entry(list, number, entry) || !SquirrelText::Field(entry, key, value))
		return std::string();

	return SquirrelText::Unquoted(value);
}

std::string Squeezed(const std::string& text)
{
	std::string squeezed;

	for (char letter : text)
	{
		if (strchr(" \t\r\n", letter) == nullptr)
			squeezed += letter;
	}

	return squeezed;
}

std::string LineValue(const std::string& line)
{
	const size_t equals = line.find('=');
	const size_t first = equals == std::string::npos ? equals : line.find_first_not_of(" \t", equals + 1);
	const size_t last = line.find_last_not_of(" \t\r\n");

	return first == std::string::npos || last < first ? std::string() : line.substr(first, last - first + 1);
}

bool SameEntry(const std::string& entry, const std::string& list, int number)
{
	std::string other;

	return BgTable::Entry(list, number, other) && Squeezed(other) == Squeezed(entry);
}

bool SameName(const std::string& line, const std::string& names, int number)
{
	std::string other;

	return BgTable::NameLine(names, number, other) && LineValue(other) == LineValue(line);
}

void Append(std::vector<int>& listed, const std::vector<int>& defined)
{
	for (int number : defined)
	{
		if (!Contains(listed, number))
			listed.push_back(number);
	}
}

std::vector<int> Ordered(const std::vector<int>& wanted, const std::vector<int>& listed, const std::vector<int>& known)
{
	std::vector<int> ordered;

	for (int number : wanted)
	{
		if (Contains(known, number) && !Contains(ordered, number))
			ordered.push_back(number);
	}

	for (int number : listed)
		PickerOrder::Place(ordered, number, listed);

	return ordered;
}

}

std::vector<int> BgTable::Numbers(const std::string& list)
{
	std::vector<int> numbers;
	const size_t mark = strlen(kMark);

	for (size_t at = list.find(kMark); at != std::string::npos; at = list.find(kMark, at + 1))
	{
		const size_t digits = at + mark;

		if (!DigitsAt(list, digits) || !AssignedAfter(list, digits + kDigits))
			continue;

		const int number = atoi(list.substr(digits, kDigits).c_str());

		if (!Contains(numbers, number))
			numbers.push_back(number);
	}

	return numbers;
}

bool BgTable::Entry(const std::string& list, int number, std::string& out)
{
	size_t start = 0;
	size_t close = 0;

	if (!BlockSpan(list, number, start, close))
		return false;

	out = list.substr(start, close - start) + kLineEnd;
	return true;
}

void BgTable::Put(std::string& list, int number, const std::string& entry)
{
	size_t start = 0;
	size_t close = 0;

	if (BlockSpan(list, number, start, close))
	{
		list.replace(start, close - start, Unlined(entry));
		return;
	}

	const size_t end = TableEnd(list);

	list.insert(end == std::string::npos ? list.size() : end, Lined(entry));
}

void BgTable::Drop(std::string& list, int number)
{
	size_t start = 0;
	size_t close = 0;

	if (!BlockSpan(list, number, start, close))
		return;

	while (close < list.size() && (list[close] == '\r' || list[close] == '\n'))
		++close;

	list.erase(start, close - start);
}

bool BgTable::Listed(const std::string& list, std::vector<int>& out)
{
	out.clear();

	size_t open = 0;
	size_t close = 0;

	if (!ListSpan(list, open, close))
		return false;

	for (size_t at = open + 1; at + 1 < close; ++at)
	{
		if (isdigit(static_cast<unsigned char>(list[at])) == 0)
			continue;

		out.push_back(atoi(list.c_str() + at));

		while (at + 1 < close && isdigit(static_cast<unsigned char>(list[at])) != 0)
			++at;
	}

	return true;
}

void BgTable::SetListed(std::string& list, const std::vector<int>& numbers)
{
	size_t open = 0;
	size_t close = 0;

	if (!ListSpan(list, open, close))
		return;

	std::string written = " ";

	for (size_t i = 0; i < numbers.size(); ++i)
	{
		written += std::to_string(numbers[i]);
		written += i + 1 < numbers.size() ? ", " : " ";
	}

	list.replace(open + 1, close - open - 2, written);
}

std::string BgTable::DataFile(const std::string& list, int number)
{
	return Value(list, number, "DataFile");
}

std::string BgTable::Name(const std::string& list, int number)
{
	return Value(list, number, "Name");
}

std::string BgTable::Key(const std::string& list, int number)
{
	std::string key = DataFile(list, number);

	if (key.empty())
		return std::to_string(number);

	std::transform(key.begin(), key.end(), key.begin(),
		[](char letter) { return static_cast<char>(tolower(static_cast<unsigned char>(letter))); });

	return key;
}

std::vector<int> BgTable::Named(const std::string& names)
{
	std::vector<int> numbers;

	for (size_t line = 0; line < names.size();)
	{
		const size_t next = names.find('\n', line);

		if (DigitsAt(names, line) && AssignedAfter(names, line + kDigits))
		{
			const int number = atoi(names.substr(line, kDigits).c_str());

			if (!Contains(numbers, number))
				numbers.push_back(number);
		}

		line = next == std::string::npos ? names.size() : next + 1;
	}

	return numbers;
}

bool BgTable::HasName(const std::string& names, int number)
{
	size_t start = 0;
	size_t end = 0;

	return LineSpan(names, number, start, end);
}

bool BgTable::NameLine(const std::string& names, int number, std::string& out)
{
	size_t start = 0;
	size_t end = 0;

	if (!LineSpan(names, number, start, end))
		return false;

	out = Lined(names.substr(start, end - start));
	return true;
}

void BgTable::PutNameLine(std::string& names, int number, const std::string& line)
{
	size_t start = 0;
	size_t end = 0;

	if (LineSpan(names, number, start, end))
	{
		names.replace(start, end - start, Lined(line));
		return;
	}

	if (!names.empty() && names.back() != '\n')
		names += kLineEnd;

	names += Lined(line);
}

void BgTable::SetName(std::string& names, int number, const std::string& name)
{
	PutNameLine(names, number, Padded(number) + " = " + name);
}

void BgTable::ClearName(std::string& names, int number)
{
	size_t start = 0;
	size_t end = 0;

	if (LineSpan(names, number, start, end))
		names.erase(start, end - start);
}

void BgTable::Stack(Text& composed, const Text& layer)
{
	const std::vector<int> defined = Numbers(layer.list);

	for (int number : defined)
	{
		std::string entry;

		if (Entry(layer.list, number, entry))
			Put(composed.list, number, entry);
	}

	for (int number : Named(layer.names))
	{
		std::string line;

		if (NameLine(layer.names, number, line))
			PutNameLine(composed.names, number, line);
	}

	std::vector<int> listed;

	if (!Listed(composed.list, listed))
		return;

	std::vector<int> wanted;

	if (!Listed(layer.list, wanted))
	{
		Append(listed, defined);
		SetListed(composed.list, listed);
		return;
	}

	SetListed(composed.list, Ordered(wanted, listed, Numbers(composed.list)));
}

int BgTable::TopListing(const std::vector<Text>& layers, std::vector<int>& outListed)
{
	for (size_t at = layers.size(); at-- > 0;)
	{
		if (Listed(layers[at].list, outListed) && !outListed.empty())
			return static_cast<int>(at);
	}

	outListed.clear();
	return -1;
}

BgTable::Text BgTable::Fragment(const Text& from, const std::vector<int>& numbers)
{
	Text fragment;

	for (int number : numbers)
	{
		std::string entry;
		std::string line;

		if (Entry(from.list, number, entry))
			fragment.list += entry;

		if (NameLine(from.names, number, line))
			fragment.names += line;
	}

	return fragment;
}

BgTable::Text BgTable::Edits(const Text& old, const Text& base, const Text& made, const std::vector<int>& numbers)
{
	Text edits;

	for (int number : numbers)
	{
		std::string entry;
		std::string line;

		if (Entry(old.list, number, entry) && !SameEntry(entry, base.list, number) &&
			!SameEntry(entry, made.list, number))
		{
			edits.list += entry;
		}

		if (NameLine(old.names, number, line) && !SameName(line, base.names, number) &&
			!SameName(line, made.names, number))
		{
			edits.names += line;
		}
	}

	return edits;
}

std::vector<int> BgTable::DropKeyed(Text& text, const std::vector<std::string>& keys)
{
	std::vector<int> dropped;

	for (int number : Numbers(text.list))
	{
		if (std::find(keys.begin(), keys.end(), Key(text.list, number)) == keys.end())
			continue;

		Drop(text.list, number);
		ClearName(text.names, number);
		dropped.push_back(number);
	}

	return dropped;
}

std::vector<std::string> BgTable::Keys(const std::string& list, const std::string& fallback,
	const std::vector<int>& numbers)
{
	std::vector<std::string> keys;

	for (int number : numbers)
	{
		std::string entry;
		keys.push_back(Key(Entry(list, number, entry) ? list : fallback, number));
	}

	return keys;
}
