#include "Game/Stages/StageFields.h"

#include "Core/Config/IniStore.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Stages/StageArchive.h"
#include "Game/Stages/StageLibrary.h"
#include "Game/Stages/StageSettings.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr const char* kSection = "StageFields";
constexpr const char* kAbsent = "-";
constexpr DWORD kSectionBytes = 16 * 1024;

struct Original
{
	std::string key;
	std::string value;
};

bool Load(int id, std::string& out)
{
	out.clear();

	std::vector<uint8_t> data;

	if (!ReadWholeFile(StageSettings::NoteOf(id), data) || data.empty())
		return StageLibrary::GameOwns(id);

	out.assign(data.begin(), data.end());
	return true;
}

bool Save(int id, const std::string& note)
{
	FILE* file = nullptr;

	if (fopen_s(&file, StageSettings::NoteOf(id).c_str(), "wb") != 0 || file == nullptr)
		return false;

	const bool written = fwrite(note.data(), 1, note.size(), file) == note.size();
	fclose(file);

	return written;
}

void Put(std::string& note, const char* key, const std::string& value)
{
	size_t valueAt = 0;
	size_t valueEnd = 0;

	if (StageArchive::FieldSpan(note, key, valueAt, valueEnd))
	{
		note.replace(valueAt, valueEnd - valueAt, value);
		return;
	}

	if (!note.empty() && note.back() != '\n')
		note += "\r\n";

	note += std::string("\t") + key + " = " + value + ",\r\n";
}

void Drop(std::string& note, const char* key)
{
	size_t valueAt = 0;
	size_t valueEnd = 0;

	if (!StageArchive::FieldSpan(note, key, valueAt, valueEnd))
		return;

	const size_t newline = note.rfind('\n', valueAt);
	const size_t lineStart = newline == std::string::npos ? 0 : newline + 1;
	const size_t lineEnd = note.find('\n', valueEnd);

	note.erase(lineStart, lineEnd == std::string::npos ? std::string::npos : lineEnd + 1 - lineStart);
}

std::vector<Original> Originals(int id)
{
	std::vector<char> buffer(kSectionBytes);
	const DWORD length = Ini::GetSection(kSection, buffer.data(), kSectionBytes,
		StageSettings::PathOf(id).c_str());

	std::vector<Original> out;

	for (const char* at = buffer.data(); at < buffer.data() + length && *at != 0;
		at += strlen(at) + 1)
	{
		const char* const equals = strchr(at, '=');

		if (equals != nullptr)
			out.push_back({ std::string(at, equals), std::string(equals + 1) });
	}

	return out;
}

void KeepOriginal(int id, const char* key, const std::string& note)
{
	if (!StageSettings::Read(id, kSection, key).empty())
		return;

	std::string value;

	if (!StageArchive::Field(note, key, value))
		value = kAbsent;

	StageSettings::Write(id, kSection, key, value);
}

}

std::string StageFields::Note(int id)
{
	std::string note;
	Load(id, note);

	return note;
}

bool StageFields::Read(int id, const char* key, std::string& out)
{
	std::string note;

	return Load(id, note) && StageArchive::Field(note, key, out);
}

bool StageFields::Write(int id, const char* key, const std::string& value)
{
	std::string note;

	if (key == nullptr || value.empty() || !Load(id, note))
		return false;

	KeepOriginal(id, key, note);
	Put(note, key, value);

	if (!Save(id, note))
	{
		LOG("StageFields: %s could not be written", StageSettings::NoteOf(id).c_str());
		return false;
	}

	LOG("StageFields: bg%03d %s = %s", id, key, value.c_str());
	return true;
}

std::vector<std::string> StageFields::Edited(int id)
{
	std::vector<std::string> keys;

	for (const Original& original : Originals(id))
		keys.push_back(original.key);

	return keys;
}

bool StageFields::Reset(int id, const std::vector<std::string>& keys)
{
	std::string note;

	if (keys.empty() || !Load(id, note))
		return false;

	int restored = 0;

	for (const Original& original : Originals(id))
	{
		if (std::find(keys.begin(), keys.end(), original.key) == keys.end())
			continue;

		if (original.value == kAbsent)
			Drop(note, original.key.c_str());
		else
			Put(note, original.key.c_str(), original.value);

		++restored;
	}

	if (restored == 0 || !Save(id, note))
		return false;

	for (const std::string& key : keys)
		StageSettings::Write(id, kSection, key.c_str(), std::string());

	LOG("StageFields: bg%03d is back to the value(s) it came with for %d setting(s)", id, restored);
	return true;
}
