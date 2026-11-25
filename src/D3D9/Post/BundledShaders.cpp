#include "D3D9/Post/BundledShaders.h"

#include "Addons/AddonManager.h"
#include "Core/FileSystem.h"
#include "Core/utils.h"

#include <Windows.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char* kType = "SHADEREXAMPLE";

struct Found
{
	std::string name;
	std::vector<uint8_t> owned;
	const uint8_t* data = nullptr;
	size_t size = 0;
};

std::vector<Found> g_shaders;
bool g_scanned = false;

bool AlreadyHave(const std::string& name)
{
	for (const Found& shader : g_shaders)
	{
		if (_stricmp(shader.name.c_str(), name.c_str()) == 0)
			return true;
	}

	return false;
}

void ScanFolder(const std::string& folder)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.' || (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		const std::string name = found.cFileName;

		if (AlreadyHave(name))
			continue;

		std::vector<uint8_t> bytes;

		if (!ReadWholeFile(folder + "\\" + name, bytes) || bytes.empty())
			continue;

		Found entry;
		entry.name = name;
		entry.size = bytes.size();
		entry.owned = std::move(bytes);
		g_shaders.push_back(std::move(entry));
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

BOOL CALLBACK OnResource(HMODULE module, LPCSTR, LPSTR name, LONG_PTR)
{
	if (IS_INTRESOURCE(name))
		return TRUE;

	const std::string leaf = ResourceFileName(name);

	if (AlreadyHave(leaf))
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
	entry.name = leaf;
	entry.data = static_cast<const uint8_t*>(data);
	entry.size = size;

	g_shaders.push_back(std::move(entry));
	return TRUE;
}

void Scan()
{
	if (g_scanned)
		return;

	g_scanned = true;

	ScanFolder(GetModAssetPath("Shaders"));

	for (const std::string& addonFolder : AddonManager::EnabledFolders())
		ScanFolder(addonFolder + "\\Shaders");

	const HMODULE module = GetModModuleHandle();

	if (module != nullptr)
		EnumResourceNamesA(module, kType, &OnResource, 0);
}

}

int BundledShaders::Count()
{
	Scan();

	return static_cast<int>(g_shaders.size());
}

const char* BundledShaders::Name(int index)
{
	if (index < 0 || index >= Count())
		return "";

	return g_shaders[index].name.c_str();
}

bool BundledShaders::Get(int index, const uint8_t*& outData, size_t& outSize)
{
	outData = nullptr;
	outSize = 0;

	if (index < 0 || index >= Count())
		return false;

	const Found& entry = g_shaders[index];
	outData = entry.owned.empty() ? entry.data : entry.owned.data();
	outSize = entry.size;
	return outData != nullptr && outSize > 0;
}
