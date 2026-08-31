#include "Game/BundledPack.h"

#include "Core/utils.h"

#include <Windows.h>

#include <cstring>
#include <vector>

namespace {

constexpr const char* kType = "PATCHPACK";

struct Found
{
	std::string id;
	const uint8_t* data;
	size_t size;
};

std::vector<Found> g_packs;
bool g_scanned = false;

BOOL CALLBACK OnResource(HMODULE module, LPCSTR, LPSTR name, LONG_PTR)
{
	if (IS_INTRESOURCE(name))
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
	entry.id = name;
	entry.data = static_cast<const uint8_t*>(data);
	entry.size = size;

	g_packs.push_back(entry);
	return TRUE;
}

void Scan()
{
	if (g_scanned)
		return;

	g_scanned = true;

	const HMODULE module = GetModModuleHandle();

	if (module == nullptr)
		return;

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

	outData = g_packs[index].data;
	outSize = g_packs[index].size;
	return true;
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
