#include "Game/Patches/BundledPack.h"

#include "Addons/AddonManager.h"
#include "Core/FileSystem.h"
#include "Core/utils.h"

#include <Windows.h>

#include <cstring>
#include <vector>

namespace {

constexpr const char* kType = "PATCHPACK";

struct Found
{
	std::string id;
	std::vector<uint8_t> owned;
	const uint8_t* data = nullptr;
	size_t size = 0;
};

std::vector<Found> g_packs;
bool g_scanned = false;

bool AlreadyHave(const std::string& id)
{
	for (const Found& pack : g_packs)
	{
		if (_stricmp(pack.id.c_str(), id.c_str()) == 0)
			return true;
	}

	return false;
}

void ScanFolder(const std::string& folder)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*.pack").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		const std::string id = found.cFileName;

		if (AlreadyHave(id))
			continue;

		std::vector<uint8_t> bytes;

		if (!ReadWholeFile(folder + "\\" + id, bytes) || bytes.empty())
			continue;

		Found entry;
		entry.id = id;
		entry.size = bytes.size();
		entry.owned = std::move(bytes);
		g_packs.push_back(std::move(entry));
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

BOOL CALLBACK OnResource(HMODULE module, LPCSTR, LPSTR name, LONG_PTR)
{
	if (IS_INTRESOURCE(name))
		return TRUE;

	const std::string id = ResourceFileName(name);

	if (AlreadyHave(id))
		return TRUE;

	const HRSRC found = FindResourceA(module, name, kType);

	if (found == nullptr)
		return TRUE;

	const DWORD size = SizeofResource(module, found);
	const HGLOBAL loaded = LoadResource(module, found);

	if (size == 0 || loaded == nullptr)
		return TRUE;

	const void* const data = LockResource(loaded);

	if (data == nullptr)
		return TRUE;

	Found entry;
	entry.id = id;
	entry.data = static_cast<const uint8_t*>(data);
	entry.size = size;

	g_packs.push_back(std::move(entry));
	return TRUE;
}

void Scan()
{
	if (g_scanned)
		return;

	g_scanned = true;

	ScanFolder(GetModAssetPath("Patches"));

	for (const std::string& addonFolder : AddonManager::EnabledFolders())
		ScanFolder(addonFolder + "\\Patches");

	const HMODULE module = GetModModuleHandle();

	if (module != nullptr)
		EnumResourceNamesA(module, kType, &OnResource, 0);
}

}

int BundledPack::Count()
{
	Scan();

	return static_cast<int>(g_packs.size());
}

bool BundledPack::Get(int index, const uint8_t*& outData, size_t& outSize)
{
	outData = nullptr;
	outSize = 0;

	if (index < 0 || index >= Count())
		return false;

	const Found& entry = g_packs[index];
	outData = entry.owned.empty() ? entry.data : entry.owned.data();
	outSize = entry.size;
	return outData != nullptr && outSize > 0;
}

int BundledPack::Find(const char* id)
{
	if (id == nullptr || id[0] == 0)
		return -1;

	for (int i = 0; i < Count(); ++i)
	{
		if (_stricmp(g_packs[i].id.c_str(), id) == 0)
			return i;
	}

	return -1;
}

const char* BundledPack::Id(int index)
{
	if (index < 0 || index >= Count())
		return "";

	return g_packs[index].id.c_str();
}
