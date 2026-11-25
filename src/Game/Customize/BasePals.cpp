#include "Game/Customize/BasePals.h"

#include "Addons/AddonManager.h"
#include "Core/FileSystem.h"
#include "Core/info.h"
#include "Core/utils.h"

#include <Windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr int kMaxChara = 128;
constexpr int kMaxSets = 4;
constexpr int kMaxRevisions = 4;
constexpr int kCurrentRevision = 0;

struct Entry
{
	std::vector<uint8_t> owned;
	const uint8_t* data = nullptr;
	size_t size = 0;
	bool looked = false;
};

Entry g_entries[kMaxChara][kMaxSets][kMaxRevisions] = {};

bool FileStem(int chara, int set, int revision, bool upper, char* out, int size)
{
	const int written = set == BasePals::kCharacterSet
		? sprintf_s(out, size, upper ? "CHR%03d" : "chr%03d", chara)
		: sprintf_s(out, size, upper ? "CHR%03d_P%d" : "chr%03d_p%d", chara, set);

	if (written <= 0)
		return false;

	if (revision == kCurrentRevision)
		return true;

	return sprintf_s(out + written, size - written, upper ? "_V%d" : "_v%d", revision) > 0;
}

bool LoadFromFolder(const std::string& folder, const char* fileName, Entry& entry)
{
	const std::string path = folder + "\\" + fileName;

	if (!ReadWholeFile(path, entry.owned) || entry.owned.empty())
		return false;

	entry.data = entry.owned.data();
	entry.size = entry.owned.size();
	return true;
}

bool LoadFromSubfolders(const std::string& root, const char* fileName, Entry& entry)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return false;

	bool loaded = false;

	do
	{
		if (found.cFileName[0] == '.' || (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			continue;

		const std::string folder = root + "\\" + found.cFileName + "\\BasePals";

		if (LoadFromFolder(folder, fileName, entry))
		{
			loaded = true;
			break;
		}
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
	return loaded;
}

bool LoadFromDisk(int chara, int set, int revision, Entry& entry)
{
	char stem[24] = {};

	if (!FileStem(chara, set, revision, false, stem, sizeof(stem)))
		return false;

	const std::string fileName = std::string(stem) + ".png";

	if (LoadFromFolder(GetModAssetPath("BasePals"), fileName.c_str(), entry))
		return true;

	for (const std::string& addonFolder : AddonManager::EnabledFolders())
	{
		if (LoadFromFolder(addonFolder + "\\BasePals", fileName.c_str(), entry))
			return true;
	}

	return LoadFromSubfolders(GetModRootPath("Packs"), fileName.c_str(), entry);
}

#if UNI2_IM_HAS_EMBEDS
bool LoadFromResource(int chara, int set, int revision, Entry& entry)
{
	char name[24] = {};

	if (!FileStem(chara, set, revision, true, name, sizeof(name)))
		return false;

	const HMODULE module = GetModModuleHandle();

	if (module == nullptr)
		return false;

	const HRSRC found = FindResourceA(module, name, reinterpret_cast<LPCSTR>(RT_RCDATA));

	if (found == nullptr)
		return false;

	const DWORD size = SizeofResource(module, found);
	const HGLOBAL loaded = LoadResource(module, found);

	if (size == 0 || loaded == nullptr)
		return false;

	const void* const data = LockResource(loaded);

	if (data == nullptr)
		return false;

	entry.data = static_cast<const uint8_t*>(data);
	entry.size = size;
	return true;
}
#endif

bool Find(int chara, int set, int revision, const uint8_t*& outData, size_t& outSize)
{
	outData = nullptr;
	outSize = 0;

	if (chara < 0 || chara >= kMaxChara)
		return false;

	if (set < 0 || set >= kMaxSets)
		return false;

	if (revision < 0 || revision >= kMaxRevisions)
		return false;

	Entry& entry = g_entries[chara][set][revision];

	if (!entry.looked)
	{
		entry.looked = true;

		if (!LoadFromDisk(chara, set, revision, entry))
		{
#if UNI2_IM_HAS_EMBEDS
			LoadFromResource(chara, set, revision, entry);
#endif
		}
	}

	if (entry.data == nullptr)
		return false;

	outData = entry.data;
	outSize = entry.size;
	return true;
}

}

bool BasePals::Get(int chara, int set, const uint8_t*& outData, size_t& outSize)
{
	return Find(chara, set, kCurrentRevision, outData, outSize);
}

std::vector<PngPalette::Sheet> BasePals::Revisions(int chara, int set)
{
	std::vector<PngPalette::Sheet> sheets;

	for (int revision = kCurrentRevision; revision < kMaxRevisions; ++revision)
	{
		PngPalette::Sheet sheet = {};

		if (Find(chara, set, revision, sheet.data, sheet.size))
			sheets.push_back(sheet);
	}

	return sheets;
}

bool BasePals::Has(int chara, int set)
{
	const uint8_t* data = nullptr;
	size_t size = 0;

	return Get(chara, set, data, size);
}
