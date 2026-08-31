#include "Game/BasePals.h"

#include "Core/utils.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr int kMaxChara = 128;

struct Entry
{
	const uint8_t* data;
	size_t size;
	bool looked;
};

Entry g_entries[kMaxChara] = {};

bool ResourceName(int chara, char* out, int size)
{
	return sprintf_s(out, size, "CHR%03d", chara) > 0;
}

bool Lookup(int chara, const uint8_t*& outData, size_t& outSize)
{
	char name[16] = {};

	if (!ResourceName(chara, name, sizeof(name)))
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

	outData = static_cast<const uint8_t*>(data);
	outSize = size;
	return true;
}

}

bool BasePals::Get(int chara, const uint8_t*& outData, size_t& outSize)
{
	outData = nullptr;
	outSize = 0;

	if (chara < 0 || chara >= kMaxChara)
		return false;

	Entry& entry = g_entries[chara];

	if (!entry.looked)
	{
		entry.looked = true;
		Lookup(chara, entry.data, entry.size);
	}

	if (entry.data == nullptr)
		return false;

	outData = entry.data;
	outSize = entry.size;
	return true;
}

bool BasePals::Has(int chara)
{
	const uint8_t* data = nullptr;
	size_t size = 0;

	return Get(chara, data, size);
}
