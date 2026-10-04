#include "Game/Stages/ObjectList.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kEntryKey = "data";
constexpr size_t kNumberDigits = 3;

std::string Uncommented(const std::string& text)
{
	std::string out;
	out.reserve(text.size());

	for (size_t at = 0; at < text.size(); ++at)
	{
		if (text.compare(at, 2, "//") == 0)
		{
			at = text.find('\n', at);

			if (at == std::string::npos)
				break;

			out.push_back('\n');
			continue;
		}

		if (text.compare(at, 2, "/*") == 0)
		{
			at = text.find("*/", at + 2);

			if (at == std::string::npos)
				break;

			++at;
			continue;
		}

		out.push_back(text[at]);
	}

	return out;
}

bool Number(const std::string& text, size_t at, int& out)
{
	if (at + kNumberDigits > text.size())
		return false;

	for (size_t i = at; i < at + kNumberDigits; ++i)
	{
		if (text[i] < '0' || text[i] > '9')
			return false;
	}

	out = atoi(text.substr(at, kNumberDigits).c_str());

	return true;
}

std::string Field(const std::string& record, const char* key)
{
	const std::string wanted = std::string(key) + "=";
	std::string compact;

	for (char letter : record)
	{
		if (letter != ' ' && letter != '\t' && letter != '\r' && letter != '\n')
			compact.push_back(letter);
	}

	for (size_t at = compact.find(wanted); at != std::string::npos; at = compact.find(wanted, at + 1))
	{
		const bool starts = at == 0 || compact[at - 1] == '{' || compact[at - 1] == ',';

		if (!starts)
			continue;

		const size_t value = at + wanted.size();

		if (value < compact.size() && compact[value] == '"')
		{
			const size_t close = compact.find('"', value + 1);

			return close == std::string::npos ? std::string() : compact.substr(value + 1, close - value - 1);
		}

		const size_t stop = compact.find_first_of(",}", value);

		return compact.substr(value, stop == std::string::npos ? std::string::npos : stop - value);
	}

	return std::string();
}

void Apply(const std::string& record, ObjectList::Entry& entry)
{
	const std::string tag = Field(record, "tag");

	if (tag == "frm")
	{
		entry.frames.push_back(ObjectList::Frame{ Field(record, "name"), atoi(Field(record, "wait").c_str()) });
		return;
	}

	if (tag == "prio")
	{
		entry.prio = atoi(Field(record, "val").c_str());
		return;
	}

	if (tag == "startdelay")
	{
		entry.delay = atoi(Field(record, "val").c_str());
		return;
	}

	if (tag != "startpos")
		return;

	entry.start[0] = static_cast<float>(atof(Field(record, "x").c_str()));
	entry.start[1] = static_cast<float>(atof(Field(record, "y").c_str()));
	entry.start[2] = static_cast<float>(atof(Field(record, "z").c_str()));
}

bool ReadEntry(const std::string& text, size_t& at, ObjectList::Entry& out)
{
	out = ObjectList::Entry();

	if (!Number(text, at + strlen(kEntryKey), out.number))
		return false;

	const size_t equals = text.find_first_not_of(" \t\r\n", at + strlen(kEntryKey) + kNumberDigits);

	if (equals == std::string::npos || text[equals] != '=')
		return false;

	const size_t open = text.find_first_not_of(" \t\r\n", equals + 1);

	if (open == std::string::npos || text[open] != '[')
		return false;

	const size_t close = text.find(']', open);

	if (close == std::string::npos)
		return false;

	for (size_t record = text.find('{', open); record != std::string::npos && record < close;
		record = text.find('{', record + 1))
	{
		const size_t end = text.find('}', record);

		if (end == std::string::npos || end > close)
			break;

		Apply(text.substr(record, end - record + 1), out);
	}

	at = close;

	return true;
}

}

std::vector<ObjectList::Entry> ObjectList::Read(const std::string& text)
{
	const std::string plain = Uncommented(text);
	std::vector<Entry> out;

	for (size_t at = plain.find(kEntryKey); at != std::string::npos; at = plain.find(kEntryKey, at + 1))
	{
		Entry entry;

		if (ReadEntry(plain, at, entry))
			out.push_back(entry);
	}

	std::stable_sort(out.begin(), out.end(), [](const Entry& left, const Entry& right)
	{
		return left.number < right.number;
	});

	if (out.size() > static_cast<size_t>(kMostEntries))
		out.resize(kMostEntries);

	return out;
}
