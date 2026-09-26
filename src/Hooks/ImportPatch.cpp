#include "Hooks/ImportPatch.h"

#include <cstring>

namespace {

template <typename T>
T* At(HMODULE module, DWORD rva)
{
	return reinterpret_cast<T*>(reinterpret_cast<BYTE*>(module) + rva);
}

const IMAGE_IMPORT_DESCRIPTOR* FirstImport(HMODULE module)
{
	const IMAGE_DOS_HEADER* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return nullptr;

	const IMAGE_NT_HEADERS* const nt = At<const IMAGE_NT_HEADERS>(module, dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return nullptr;

	const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (directory.VirtualAddress == 0)
		return nullptr;

	return At<const IMAGE_IMPORT_DESCRIPTOR>(module, directory.VirtualAddress);
}

const IMAGE_IMPORT_DESCRIPTOR* FindDll(HMODULE module, const char* dllName)
{
	for (const IMAGE_IMPORT_DESCRIPTOR* entry = FirstImport(module); entry != nullptr && entry->Name != 0; ++entry)
	{
		if (_stricmp(At<const char>(module, entry->Name), dllName) == 0)
			return entry;
	}

	return nullptr;
}

IMAGE_THUNK_DATA* FindSlot(HMODULE module, const IMAGE_IMPORT_DESCRIPTOR* dll, const char* functionName)
{
	if (dll->OriginalFirstThunk == 0)
		return nullptr;

	const IMAGE_THUNK_DATA* name = At<const IMAGE_THUNK_DATA>(module, dll->OriginalFirstThunk);
	IMAGE_THUNK_DATA* slot = At<IMAGE_THUNK_DATA>(module, dll->FirstThunk);

	for (; name->u1.AddressOfData != 0; ++name, ++slot)
	{
		if (IMAGE_SNAP_BY_ORDINAL(name->u1.Ordinal))
			continue;

		const IMAGE_IMPORT_BY_NAME* const byName =
			At<const IMAGE_IMPORT_BY_NAME>(module, static_cast<DWORD>(name->u1.AddressOfData));

		if (strcmp(reinterpret_cast<const char*>(byName->Name), functionName) == 0)
			return slot;
	}

	return nullptr;
}

}

bool ImportPatch::Replace(HMODULE module, const char* dllName, const char* functionName, void* replacement,
	void** outOriginal)
{
	if (module == nullptr)
		return false;

	const IMAGE_IMPORT_DESCRIPTOR* const dll = FindDll(module, dllName);
	if (dll == nullptr)
		return false;

	IMAGE_THUNK_DATA* const slot = FindSlot(module, dll, functionName);
	if (slot == nullptr)
		return false;

	DWORD oldProtect = 0;
	if (!VirtualProtect(&slot->u1.Function, sizeof(slot->u1.Function), PAGE_READWRITE, &oldProtect))
		return false;

	*outOriginal = reinterpret_cast<void*>(slot->u1.Function);
	slot->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);

	VirtualProtect(&slot->u1.Function, sizeof(slot->u1.Function), oldProtect, &oldProtect);
	return true;
}
