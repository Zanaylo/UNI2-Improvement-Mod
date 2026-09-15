#include "D3D9/GraphicsWrapper.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/D3D9Proxy.h"

#include <Windows.h>
#include <d3d9.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "version.lib")

namespace {

bool g_asked = false;
bool g_wrapper = false;

char g_module[MAX_PATH] = "";
char g_name[96] = "";
char g_status[320] = "not asked yet";

const char* LastSegment(const char* path)
{
	const char* const slash = strrchr(path, '\\');
	return slash != nullptr ? slash + 1 : path;
}

bool UnderSystemDirectory(const char* path)
{
	const std::string system = GetSystemDirectoryPath();

	if (system.empty() || system.size() >= strlen(path))
		return false;

	return _strnicmp(path, system.c_str(), system.size()) == 0;
}

HMODULE OwnerOfVTable(IDirect3DDevice9* device)
{
	if (device == nullptr)
		return nullptr;

	void* vtable = nullptr;

	if (!TryReadMemory(&vtable, device, sizeof(vtable)) || vtable == nullptr)
		return nullptr;

	HMODULE owner = nullptr;

	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<const char*>(vtable),
		&owner) == 0)
	{
		return nullptr;
	}

	return owner;
}

HMODULE LoadedDirect3D9()
{
	const HMODULE module = GetModuleHandleA("d3d9.dll");

	if (module == nullptr || module != GetModModuleHandle())
		return module;

	return D3D9Proxy::RealModule();
}

bool ReadProductName(const char* path, char* out, int size)
{
	DWORD ignored = 0;
	const DWORD bytes = GetFileVersionInfoSizeA(path, &ignored);

	if (bytes == 0)
		return false;

	std::vector<uint8_t> block(bytes);

	if (GetFileVersionInfoA(path, 0, bytes, block.data()) == 0)
		return false;

	struct Language
	{
		WORD language;
		WORD codePage;
	};

	Language* languages = nullptr;
	UINT languageBytes = 0;

	if (VerQueryValueA(block.data(), "\\VarFileInfo\\Translation",
		reinterpret_cast<void**>(&languages), &languageBytes) == 0 ||
		languageBytes < sizeof(Language))
	{
		return false;
	}

	char key[64] = {};
	sprintf_s(key, "\\StringFileInfo\\%04x%04x\\ProductName", languages[0].language,
		languages[0].codePage);

	char* text = nullptr;
	UINT textBytes = 0;

	if (VerQueryValueA(block.data(), key, reinterpret_cast<void**>(&text), &textBytes) == 0 ||
		text == nullptr || text[0] == 0)
	{
		return false;
	}

	strncpy_s(out, size, text, _TRUNCATE);
	return true;
}

void Settle(HMODULE owner)
{
	if (owner == nullptr)
	{
		strncpy_s(g_status, "the Direct3D 9 device belongs to no module the loader knows",
			_TRUNCATE);
		return;
	}

	if (GetModuleFileNameA(owner, g_module, sizeof(g_module)) == 0)
	{
		strncpy_s(g_status, "the Direct3D 9 module has no file name", _TRUNCATE);
		return;
	}

	g_wrapper = !UnderSystemDirectory(g_module);

	if (!ReadProductName(g_module, g_name, sizeof(g_name)))
		strncpy_s(g_name, LastSegment(g_module), _TRUNCATE);

	if (!g_wrapper)
	{
		sprintf_s(g_status, "Direct3D 9 is the system's own");
		return;
	}

	sprintf_s(g_status, "Direct3D 9 is %s, out of %s", g_name, g_module);
}

}

void GraphicsWrapper::Detect(IDirect3DDevice9* device)
{
	if (g_asked)
		return;

	g_asked = true;

	HMODULE owner = OwnerOfVTable(device);

	if (owner == nullptr)
		owner = LoadedDirect3D9();

	Settle(owner);

	LOG("GraphicsWrapper: %s", g_status);
}

bool GraphicsWrapper::IsPresent()
{
	return g_wrapper;
}

const char* GraphicsWrapper::Name()
{
	return g_name;
}

const char* GraphicsWrapper::StatusText()
{
	return g_status;
}
