#include "D3D9/Post/BundledShaders.h"

#include "Core/utils.h"

#include <Windows.h>

#include <string>
#include <vector>

namespace {

constexpr const char* kType = "SHADEREXAMPLE";

struct Found
{
	std::string name;
	const uint8_t* data;
	size_t size;
};

std::vector<Found> g_shaders;
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
	entry.name = ResourceFileName(name);
	entry.data = static_cast<const uint8_t*>(data);
	entry.size = size;

	g_shaders.push_back(entry);
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

	outData = g_shaders[index].data;
	outSize = g_shaders[index].size;
	return true;
}
