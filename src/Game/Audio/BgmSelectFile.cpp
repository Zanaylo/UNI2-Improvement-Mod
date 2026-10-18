#include "Game/Audio/BgmSelectFile.h"

#include "Core/FileSystem.h"
#include "Core/utils.h"
#include "Game/Files/DataArchive.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kModFile = "Mods\\grpdat\\CSel\\bgmselect.txt";
constexpr const char* kArchiveFolder = "CSel";
constexpr const char* kArchiveFile = "bgmselect.txt";
constexpr const char* kNumberKey = "num";
constexpr const char* kLineEnd = "\r\n";
constexpr int kGameDefaultNumber = 1;

std::string Trimmed(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t");

	if (first == std::string::npos)
		return std::string();

	const size_t last = text.find_last_not_of(" \t");
	return text.substr(first, last - first + 1);
}

std::vector<std::string> Lines(const std::vector<uint8_t>& blob)
{
	std::vector<std::string> out;
	std::string current;

	for (const uint8_t byte : blob)
	{
		if (byte == '\n')
		{
			out.push_back(current);
			current.clear();
			continue;
		}

		if (byte != '\r')
			current.push_back(static_cast<char>(byte));
	}

	if (!current.empty())
		out.push_back(current);

	return out;
}

bool IsHeader(const std::string& line)
{
	return line.size() > 2 && line.front() == '[' && line.back() == ']';
}

bool ReadNumber(const std::string& line, int& out)
{
	const size_t equals = line.find('=');

	if (equals == std::string::npos)
		return false;

	if (_stricmp(Trimmed(line.substr(0, equals)).c_str(), kNumberKey) != 0)
		return false;

	out = atoi(line.c_str() + equals + 1);
	return true;
}

}

std::vector<uint8_t> BgmSelectFile::ReadBase()
{
	std::vector<uint8_t> blob;

	if (ReadWholeFile(GetModRootPath(kModFile), blob))
		return blob;

	DataArchive::Read(kArchiveFolder, kArchiveFile, blob);
	return blob;
}

void BgmSelectFile::Split(const std::vector<uint8_t>& blob, Contents& out)
{
	out.prefix.clear();
	out.records.clear();

	Record* open = nullptr;

	for (const std::string& raw : Lines(blob))
	{
		const std::string line = Trimmed(raw);

		if (IsHeader(line))
		{
			out.records.push_back({ kGameDefaultNumber, std::string() });
			open = &out.records.back();
			continue;
		}

		if (open == nullptr)
		{
			out.prefix += raw + kLineEnd;
			continue;
		}

		open->body += raw + kLineEnd;
		ReadNumber(line, open->number);
	}
}

std::string BgmSelectFile::Compose(const Contents& contents)
{
	std::string text = contents.prefix;

	for (size_t i = 0; i < contents.records.size(); ++i)
	{
		char header[16] = {};
		sprintf_s(header, "[%02d]%s", static_cast<int>(i), kLineEnd);
		text += header;
		text += contents.records[i].body;
	}

	return text;
}

std::string BgmSelectFile::Body(int number, const std::string& name)
{
	return std::string(kNumberKey) + " = " + std::to_string(number) + kLineEnd + "name = " + name +
		kLineEnd + kLineEnd;
}
