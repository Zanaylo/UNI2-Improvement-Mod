#include "Game/Customize/AnnouncerList.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>

namespace {

constexpr size_t kFolderField = 3;
constexpr size_t kSaveField = 4;
constexpr size_t kRowFields = 6;
constexpr int kAlwaysUnlocked = 0;
constexpr int kSystemRoundCall = 27;
constexpr int kSaveSlots = 64;

constexpr int kColumns = 11;
constexpr int kLeft = -512;
constexpr int kStepX = 52;
constexpr int kTop = -180;
constexpr int kStepY = 138;
constexpr int kShownRows = 3;
constexpr int kScreenCentreY = 360;
constexpr int kFirstDelay = 1;
constexpr int kDelayStep = 2;
constexpr const char* kFrames = "4";

std::string NewlineOf(const std::string& text)
{
	return text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
}

std::vector<std::string> Lines(const std::string& text)
{
	std::vector<std::string> lines;
	std::istringstream stream(text);
	std::string line;

	while (std::getline(stream, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		lines.push_back(line);
	}

	return lines;
}

std::vector<std::string> Fields(const std::string& row)
{
	std::vector<std::string> fields;
	std::istringstream stream(row);
	std::string field;

	while (std::getline(stream, field, ','))
		fields.push_back(field);

	return fields;
}

std::string Trimmed(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t");
	const size_t last = text.find_last_not_of(" \t");

	return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
}

std::vector<std::vector<std::string>> Rows(const std::string& csv)
{
	std::vector<std::vector<std::string>> rows;
	const std::vector<std::string> lines = Lines(csv);

	for (size_t i = 1; i < lines.size(); ++i)
	{
		std::vector<std::string> fields = Fields(lines[i]);

		if (fields.size() >= kRowFields)
			rows.push_back(fields);
	}

	return rows;
}

bool Claims(const std::vector<AnnouncerList::Entry>& entries, const std::string& folder)
{
	return std::any_of(entries.begin(), entries.end(),
		[&folder](const AnnouncerList::Entry& entry) { return entry.folder == folder; });
}

bool Numbered(const std::vector<AnnouncerList::Entry>& entries, int number)
{
	return std::any_of(entries.begin(), entries.end(),
		[number](const AnnouncerList::Entry& entry) { return entry.number == number; });
}

std::string Row(const AnnouncerList::Entry& entry)
{
	char row[256] = {};
	sprintf_s(row, "%s,%d,%d,%s,%d,%d", entry.name.c_str(), entry.number, kAlwaysUnlocked,
		entry.folder.c_str(), entry.saveId, kSystemRoundCall);

	return row;
}

bool IsSectionHeader(const std::string& line)
{
	const std::string trimmed = Trimmed(line);
	return !trimmed.empty() && trimmed[0] == '[';
}

int SectionNumber(const std::string& line)
{
	return atoi(Trimmed(line).c_str() + 1);
}

std::vector<std::string> Section(const AnnouncerList::Entry& entry)
{
	char header[16] = {};
	sprintf_s(header, "[%03d]", entry.number);

	const int x = kLeft + (entry.cell % kColumns) * kStepX;
	const int y = kTop + (entry.cell / kColumns) * kStepY;
	const int delay = kFirstDelay + entry.cell * kDelayStep;

	return {
		header,
		"icon = " + entry.icon,
		"position_x = " + std::to_string(x),
		"position_y = " + std::to_string(y),
		"delay = " + std::to_string(delay),
		std::string("icon_animation_allpattern = ") + kFrames,
	};
}

}

int AnnouncerList::RowCount(const std::string& csv)
{
	return static_cast<int>(Rows(csv).size());
}

int AnnouncerList::Capacity(const std::string& csv)
{
	return std::max(0, kSaveSlots - RowCount(csv));
}

int AnnouncerList::WindowBottom()
{
	return (kShownRows - 1) * kStepY;
}

int AnnouncerList::RowOfIcon(int baseY)
{
	return (baseY - kTop - kScreenCentreY) / kStepY;
}

int AnnouncerList::ShownY(int baseY, int scroll)
{
	const int fromTop = baseY - kTop - kScreenCentreY - scroll;

	return fromTop >= 0 && fromTop <= WindowBottom() ? baseY - scroll : kHiddenY;
}

std::vector<int> AnnouncerList::SaveIds(const std::string& csv)
{
	std::vector<int> ids;

	for (const std::vector<std::string>& row : Rows(csv))
		ids.push_back(atoi(row[kSaveField].c_str()));

	return ids;
}

std::vector<int> AnnouncerList::FreeSaveIds(const std::string& csv, int count)
{
	const std::vector<int> listed = SaveIds(csv);
	const std::set<int> taken(listed.begin(), listed.end());
	std::vector<int> free;

	for (int id = kSaveSlots - 1; id >= 0 && static_cast<int>(free.size()) < count; --id)
	{
		if (taken.count(id) == 0)
			free.push_back(id);
	}

	return free;
}

std::string AnnouncerList::WithRows(const std::string& csv, const std::vector<Entry>& entries)
{
	const std::string newline = NewlineOf(csv);
	const std::vector<std::string> lines = Lines(csv);
	std::string out;

	for (size_t i = 0; i < lines.size(); ++i)
	{
		const std::vector<std::string> fields = Fields(lines[i]);
		const bool claimed = i > 0 && fields.size() > kFolderField && Claims(entries, Trimmed(fields[kFolderField]));

		if (lines[i].empty() || claimed)
			continue;

		out += lines[i] + newline;
	}

	for (const Entry& entry : entries)
		out += Row(entry) + newline;

	return out;
}

std::string AnnouncerList::WithIcons(const std::string& ini, const std::vector<Entry>& entries)
{
	const std::string newline = NewlineOf(ini);
	std::vector<std::string> kept;
	bool skipping = false;

	for (const std::string& line : Lines(ini))
	{
		if (IsSectionHeader(line))
			skipping = Numbered(entries, SectionNumber(line));

		if (!skipping)
			kept.push_back(line);
	}

	while (!kept.empty() && Trimmed(kept.back()).empty())
		kept.pop_back();

	std::string out;

	for (const std::string& line : kept)
		out += line + newline;

	for (const Entry& entry : entries)
	{
		out += newline;

		for (const std::string& line : Section(entry))
			out += line + newline;
	}

	return out;
}
