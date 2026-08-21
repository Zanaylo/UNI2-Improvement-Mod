#include "D3D9/D3D9Proxy.h"

#include "Core/Compat.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/D3D9Wrapper.h"

#include <d3d9.h>

#include <mutex>
#include <string>

namespace {

constexpr char kProxyName[] = "d3d9.dll";

HMODULE g_real = nullptr;
bool g_loadTried = false;
std::mutex g_loadMutex;

bool g_identityKnown = false;
bool g_isD3D9 = false;
char g_loadedAs[64] = "";

const char* FileNameOf(const char* path)
{
	const char* const slash = strrchr(path, '\\');
	return slash != nullptr ? slash + 1 : path;
}

void ResolveIdentity()
{
	if (g_identityKnown)
		return;

	g_identityKnown = true;

	char path[MAX_PATH] = {};
	if (GetModuleFileNameA(GetModModuleHandle(), path, MAX_PATH) == 0)
		return;

	const char* const name = FileNameOf(path);

	strncpy_s(g_loadedAs, name, _TRUNCATE);
	g_isD3D9 = _stricmp(name, kProxyName) == 0;
}

bool LoadRealRuntime()
{
	const std::string path = GetSystemDirectoryPath() + kProxyName;

	HMODULE loaded = LoadLibraryA(path.c_str());
	if (loaded == nullptr)
	{
		LOG("d3d9 proxy: could not load the real runtime from %s (error %lu)", path.c_str(),
			GetLastError());
		return false;
	}

	if (loaded == GetModModuleHandle())
	{
		LOG("d3d9 proxy: %s resolved back to this mod. Refusing to chain into itself.", path.c_str());
		return false;
	}

	g_real = loaded;
	LOG("d3d9 proxy: real runtime loaded from %s", path.c_str());
	return true;
}

bool EnsureRealLoaded()
{
	std::lock_guard<std::mutex> lock(g_loadMutex);

	if (g_loadTried)
		return g_real != nullptr;

	g_loadTried = true;
	return LoadRealRuntime();
}

void* RealExport(const char* name)
{
	if (!EnsureRealLoaded())
		return nullptr;

	void* const address = reinterpret_cast<void*>(GetProcAddress(g_real, name));
	if (address == nullptr)
		LOG("d3d9 proxy: the real runtime has no %s", name);

	return address;
}

template <typename T>
T RealFunction(const char* name, T& cache)
{
	if (cache == nullptr)
		cache = reinterpret_cast<T>(RealExport(name));

	return cache;
}

}

bool D3D9Proxy::IsActive()
{
	ResolveIdentity();
	return g_isD3D9;
}

HMODULE D3D9Proxy::RealModule()
{
	return EnsureRealLoaded() ? g_real : nullptr;
}

const char* D3D9Proxy::LoadedAs()
{
	ResolveIdentity();
	return g_loadedAs;
}

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT sdkVersion)
{
	using Fn = IDirect3D9*(WINAPI*)(UINT);
	static Fn cache = nullptr;

	const Fn real = RealFunction("Direct3DCreate9", cache);
	if (real == nullptr)
		return nullptr;

	IDirect3D9* const d3d9 = real(sdkVersion);

	LOG("d3d9 proxy: Direct3DCreate9(sdk %u) -> 0x%p", sdkVersion, (void*)d3d9);

	if (Compat::StoodDown())
		return d3d9;

	D3D9Wrapper::OnDirect3D9Created(d3d9);
	return d3d9;
}

extern "C" HRESULT WINAPI Direct3DCreate9Ex(UINT sdkVersion, IDirect3D9Ex** ppD3D)
{
	using Fn = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
	static Fn cache = nullptr;

	const Fn real = RealFunction("Direct3DCreate9Ex", cache);
	if (real == nullptr)
		return E_NOTIMPL;

	const HRESULT result = real(sdkVersion, ppD3D);

	if (FAILED(result) || ppD3D == nullptr || *ppD3D == nullptr)
		return result;

	LOG("d3d9 proxy: Direct3DCreate9Ex(sdk %u) -> 0x%p", sdkVersion, (void*)*ppD3D);

	if (Compat::StoodDown())
		return result;

	D3D9Wrapper::OnDirect3D9Created(reinterpret_cast<IDirect3D9*>(*ppD3D));
	return result;
}

extern "C" int WINAPI D3DPERF_BeginEvent(D3DCOLOR colour, LPCWSTR name)
{
	using Fn = int(WINAPI*)(D3DCOLOR, LPCWSTR);
	static Fn cache = nullptr;

	const Fn real = RealFunction("D3DPERF_BeginEvent", cache);
	if (real == nullptr)
		return 0;

	return real(colour, name);
}

extern "C" int WINAPI D3DPERF_EndEvent(void)
{
	using Fn = int(WINAPI*)(void);
	static Fn cache = nullptr;

	const Fn real = RealFunction("D3DPERF_EndEvent", cache);
	if (real == nullptr)
		return 0;

	return real();
}

extern "C" void WINAPI D3DPERF_SetMarker(D3DCOLOR colour, LPCWSTR name)
{
	using Fn = void(WINAPI*)(D3DCOLOR, LPCWSTR);
	static Fn cache = nullptr;

	const Fn real = RealFunction("D3DPERF_SetMarker", cache);
	if (real == nullptr)
		return;

	real(colour, name);
}

extern "C" void WINAPI D3DPERF_SetRegion(D3DCOLOR colour, LPCWSTR name)
{
	using Fn = void(WINAPI*)(D3DCOLOR, LPCWSTR);
	static Fn cache = nullptr;

	const Fn real = RealFunction("D3DPERF_SetRegion", cache);
	if (real == nullptr)
		return;

	real(colour, name);
}

extern "C" BOOL WINAPI D3DPERF_QueryRepeatFrame(void)
{
	using Fn = BOOL(WINAPI*)(void);
	static Fn cache = nullptr;

	const Fn real = RealFunction("D3DPERF_QueryRepeatFrame", cache);
	if (real == nullptr)
		return FALSE;

	return real();
}

extern "C" void WINAPI D3DPERF_SetOptions(DWORD options)
{
	using Fn = void(WINAPI*)(DWORD);
	static Fn cache = nullptr;

	const Fn real = RealFunction("D3DPERF_SetOptions", cache);
	if (real == nullptr)
		return;

	real(options);
}

extern "C" DWORD WINAPI D3DPERF_GetStatus(void)
{
	using Fn = DWORD(WINAPI*)(void);
	static Fn cache = nullptr;

	const Fn real = RealFunction("D3DPERF_GetStatus", cache);
	if (real == nullptr)
		return 0;

	return real();
}
