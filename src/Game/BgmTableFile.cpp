#include "Game/BgmTableFile.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr uint32_t kLargestTable = 256u * 1024u;
constexpr int kFirstPackSlot = 100;

std::string OverridePath()
{
	return GetModRootPath("Mods\\Bgm\\bgm.txt");
}

bool WriteWhole(const std::string& path, const std::vector<uint8_t>& data)
{
	FILE* handle = nullptr;

	if (fopen_s(&handle, path.c_str(), "wb") != 0 || handle == nullptr)
		return false;

	if (!data.empty())
		fwrite(data.data(), 1, data.size(), handle);

	fclose(handle);
	return true;
}

bool LooksLikeTable(const std::vector<uint8_t>& blob)
{
	const std::string text(reinterpret_cast<const char*>(blob.data()), blob.size());
	return text.find("IsLoop") != std::string::npos && BgmTableFile::HasVanillaSlots(blob);
}

}

bool BgmTableFile::HasVanillaSlots(const std::vector<uint8_t>& blob)
{
	const std::string text(reinterpret_cast<const char*>(blob.data()), blob.size());

	size_t at = 0;

	while ((at = text.find("[BGM_", at)) != std::string::npos)
	{
		const size_t start = at;
		at += 5;

		if (start != 0 && text[start - 1] != '\n' && text[start - 1] != '\r')
			continue;

		if (atoi(text.c_str() + at) < kFirstPackSlot)
			return true;
	}

	return false;
}

bool BgmTableFile::ReadGameTable(std::vector<uint8_t>& out)
{
	out.clear();

	const std::string folder = GetModDirectory() + "d\\";

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (found.nFileSizeHigh != 0 || found.nFileSizeLow > kLargestTable)
			continue;

		std::vector<uint8_t> blob;

		if (!ReadWholeFile(folder + found.cFileName, blob, 16))
			continue;

		if (!LooksLikeTable(blob))
			continue;

		out.swap(blob);
		LOG("BgmTableFile: the game's own slot table is d\\%s, %d bytes",
			found.cFileName, static_cast<int>(out.size()));
		break;
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return !out.empty();
}

void BgmTableFile::Repair()
{
	const std::string path = OverridePath();

	std::vector<uint8_t> current;

	if (!ReadWholeFile(path, current, 0))
		return;

	if (HasVanillaSlots(current))
		return;

	std::vector<uint8_t> vanilla;

	if (!ReadGameTable(vanilla))
	{
		LOG("BgmTableFile: %s has no vanilla slot in it and the game's own table could not be "
			"found, so the game will have no music of its own", path.c_str());
		return;
	}

	vanilla.insert(vanilla.end(), current.begin(), current.end());

	if (!WriteWhole(path, vanilla))
		return;

	LOG("BgmTableFile: %s had lost the game's own slots and has been repaired",
		path.c_str());
}
