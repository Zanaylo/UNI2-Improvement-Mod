#include "Game/SeListFile.h"

#include "Core/logger.h"
#include "Game/DataArchive.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kAppend = "SeList_Table.Path.append(";
constexpr const char* kStatus = "SeList_Table.Status.resize";

std::string ReadOriginal(int chara)
{
	char folder[16] = {};
	char file[32] = {};

	sprintf_s(folder, "chr%03d", chara);
	sprintf_s(file, "chr%03d_se_list.txt", chara);

	std::vector<uint8_t> bytes;

	if (!DataArchive::Read(folder, file, bytes) || bytes.empty())
		return std::string();

	return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

size_t LineEnd(const std::string& text, size_t at)
{
	const size_t end = text.find('\n', at);
	return end == std::string::npos ? text.size() : end + 1;
}

bool Wanted(const std::string& line, const std::vector<std::string>& stems)
{
	const size_t at = line.find("file=\"");

	if (at == std::string::npos)
		return false;

	const size_t start = at + 6;
	const size_t end = line.find('"', start);

	if (end == std::string::npos)
		return false;

	const std::string stem = line.substr(start, end - start);

	return std::find(stems.begin(), stems.end(), stem) != stems.end();
}

bool RepointLine(std::string& line, int index)
{
	const size_t at = line.find("path=");

	if (at == std::string::npos)
		return false;

	size_t end = at + 5;

	while (end < line.size() && line[end] >= '0' && line[end] <= '9')
		++end;

	if (end == at + 5)
		return false;

	char replacement[16] = {};
	sprintf_s(replacement, "path=%d", index);

	line.replace(at, end - at, replacement);
	return true;
}

}

bool SeListFile::Rewrite(int chara, const std::vector<std::string>& stems,
	const std::string& folder, std::string& out)
{
	if (stems.empty() || folder.empty())
		return false;

	const std::string original = ReadOriginal(chara);

	if (original.empty())
	{
		LOG("SeListFile: chr%03d has no sound list in the archive", chara);
		return false;
	}

	size_t lastAppend = std::string::npos;
	int appends = 0;

	for (size_t at = original.find(kAppend); at != std::string::npos;
		at = original.find(kAppend, at + 1))
	{
		lastAppend = LineEnd(original, at);
		++appends;
	}

	if (lastAppend == std::string::npos)
	{
		lastAppend = original.find(kStatus);

		if (lastAppend == std::string::npos)
		{
			LOG("SeListFile: chr%03d's sound list has no path table to extend", chara);
			return false;
		}
	}

	char line[256] = {};
	sprintf_s(line, "SeList_Table.Path.append( \"%s\" );\r\n", folder.c_str());

	out = original.substr(0, lastAppend) + line;

	int repointed = 0;

	for (size_t at = lastAppend; at < original.size();)
	{
		const size_t end = LineEnd(original, at);
		std::string current = original.substr(at, end - at);

		if (Wanted(current, stems) && RepointLine(current, appends))
			++repointed;

		out += current;
		at = end;
	}

	if (repointed == 0)
	{
		LOG("SeListFile: chr%03d plays none of the %d replaced sound(s)", chara,
			static_cast<int>(stems.size()));
		return false;
	}

	LOG("SeListFile: chr%03d - %d entry(ies) repointed at %s", chara, repointed, folder.c_str());
	return true;
}
