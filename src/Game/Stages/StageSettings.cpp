#include "Game/Stages/StageSettings.h"

#include "Core/Config/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/StageLibrary.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr const char* kLeaf = "settings.ini";
constexpr const char* kNoteLeaf = "stage.txt";
constexpr const char* kOwnFolder = "own";
constexpr const char* kHashedPrefix = "Stage_";
constexpr const char* kNumberedPrefix = "Stage";
constexpr size_t kHashLength = 16;
constexpr DWORD kSectionBytes = 64 * 1024;
constexpr DWORD kValueBytes = 256;

struct Carried
{
	const char* section;
	const char* suffix;
	const char* key;
};

const Carried kCarried[] = {
	{ "StageColour", "Glow", "Glow" },
	{ "StageColour", "Off", "Off" },
	{ "StageColour", "", "Grade" },
	{ "StagePlacement", "", "Place" },
};

struct Line
{
	std::string key;
	std::string value;
};

std::string FolderOf(int number)
{
	if (!StageLibrary::GameOwns(number))
		return StageLibrary::FolderOf(number);

	char leaf[16] = {};
	sprintf_s(leaf, "bg%03d", number);

	return StageLibrary::Root() + "\\" + kOwnFolder + "\\" + leaf;
}

bool Exists(const std::string& folder)
{
	const DWORD attributes = GetFileAttributesA(folder.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::vector<Line> SectionLines(const char* section)
{
	std::vector<char> buffer(kSectionBytes);
	const DWORD length = GetPrivateProfileSectionA(section, buffer.data(), kSectionBytes,
		Settings::GetIniPath().c_str());

	std::vector<Line> out;

	for (const char* at = buffer.data(); at < buffer.data() + length && *at != 0;
		at += strlen(at) + 1)
	{
		const char* const equals = strchr(at, '=');

		if (equals == nullptr)
			continue;

		out.push_back({ std::string(at, equals), std::string(equals + 1) });
	}

	return out;
}

bool IsHex(const std::string& text)
{
	for (const char letter : text)
	{
		if (isxdigit(static_cast<unsigned char>(letter)) == 0)
			return false;
	}

	return true;
}

int HashedStage(const std::string& hash)
{
	std::vector<StageLibrary::Entry> entries;
	StageLibrary::Snapshot(entries);

	for (const StageLibrary::Entry& entry : entries)
	{
		if (entry.key == hash)
			return entry.id;
	}

	return -1;
}

int NumberedStage(const std::string& digits, const char* section)
{
	if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos)
		return -1;

	const int number = atoi(digits.c_str());

	if (StageLibrary::GameOwns(number))
		return number;

	StageLibrary::Entry entry = {};
	const bool colourById = strcmp(section, "StageColour") == 0;

	return colourById && StageLibrary::Of(number, entry) ? number : -1;
}

int StageOf(const std::string& body, const char* section)
{
	if (body.compare(0, strlen(kHashedPrefix), kHashedPrefix) == 0)
	{
		const std::string hash = body.substr(strlen(kHashedPrefix));

		return hash.size() == kHashLength && IsHex(hash) ? HashedStage(hash) : -1;
	}

	if (body.compare(0, strlen(kNumberedPrefix), kNumberedPrefix) != 0)
		return -1;

	return NumberedStage(body.substr(strlen(kNumberedPrefix)), section);
}

bool Resolve(const std::string& iniKey, const Carried& carried, int& outStage)
{
	const size_t suffix = strlen(carried.suffix);

	if (iniKey.size() <= suffix || iniKey.compare(iniKey.size() - suffix, suffix, carried.suffix) != 0)
		return false;

	outStage = StageOf(iniKey.substr(0, iniKey.size() - suffix), carried.section);
	return outStage >= 0;
}

void Forget(const char* section, const std::string& key)
{
	Settings::SaveString(section, key.c_str(), nullptr);
}

void MigrateSection(const char* section, int& moved, int& kept)
{
	for (const Line& line : SectionLines(section))
	{
		if (line.value.empty())
		{
			Forget(section, line.key);
			continue;
		}

		int stage = -1;
		const Carried* match = nullptr;

		for (const Carried& carried : kCarried)
		{
			if (strcmp(carried.section, section) == 0 && Resolve(line.key, carried, stage))
			{
				match = &carried;
				break;
			}
		}

		if (match == nullptr)
		{
			++kept;
			LOG("StageSettings: [%s] %s has no stage to go to, it stays in the ini", section,
				line.key.c_str());
			continue;
		}

		if (StageSettings::Read(stage, section, match->key).empty()
			&& !StageSettings::Write(stage, section, match->key, line.value))
		{
			++kept;
			LOG("StageSettings: [%s] %s could not be written to bg%03d", section, line.key.c_str(),
				stage);
			continue;
		}

		Forget(section, line.key);
		++moved;
	}

	if (SectionLines(section).empty())
		Settings::SaveString(section, nullptr, nullptr);
}

}

std::string StageSettings::PathOf(int number)
{
	return FolderOf(number) + "\\" + kLeaf;
}

std::string StageSettings::NoteOf(int number)
{
	return FolderOf(number) + "\\" + kNoteLeaf;
}

std::string StageSettings::Read(int number, const char* section, const char* key)
{
	char value[kValueBytes] = {};

	GetPrivateProfileStringA(section, key, "", value, sizeof(value), PathOf(number).c_str());

	return value;
}

bool StageSettings::Write(int number, const char* section, const char* key,
	const std::string& value)
{
	const std::string folder = FolderOf(number);

	if (!Exists(folder))
	{
		if (value.empty() || !StageLibrary::GameOwns(number) || !CreateDirectoryTree(folder))
			return false;
	}

	return WritePrivateProfileStringA(section, key, value.empty() ? nullptr : value.c_str(),
		PathOf(number).c_str()) != 0;
}

void StageSettings::Migrate()
{
	int moved = 0;
	int kept = 0;

	MigrateSection("StageColour", moved, kept);
	MigrateSection("StagePlacement", moved, kept);

	if (moved == 0 && kept == 0)
		return;

	LOG("StageSettings: %d stage setting(s) moved from the ini into the stages' %s, %d stayed",
		moved, kLeaf, kept);
}
