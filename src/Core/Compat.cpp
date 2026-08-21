#include "Core/Compat.h"

#include "Core/interfaces.h"
#include "Core/logger.h"

#include <Windows.h>
#include <cstdio>
#include <cstring>

namespace {

using WineGetVersion_t = const char*(__cdecl*)(void);
using WineGetHostVersion_t = void(__cdecl*)(const char**, const char**);

bool g_detected = false;
bool g_isWine = false;
bool g_rtss = false;
bool g_stoodDown = false;

char g_wineVersion[64] = "";
char g_host[96] = "";
char g_description[256] = "not detected yet";

const char* CallWineVersion(WineGetVersion_t getVersion)
{
	__try
	{
		return getVersion();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return nullptr;
	}
}

bool CallWineHostVersion(WineGetHostVersion_t getHost, const char*& outSystem, const char*& outRelease)
{
	__try
	{
		getHost(&outSystem, &outRelease);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

void ReadWineHost(HMODULE ntdll)
{
	const auto getHost = reinterpret_cast<WineGetHostVersion_t>(
		GetProcAddress(ntdll, "wine_get_host_version"));

	if (getHost == nullptr)
		return;

	const char* system = nullptr;
	const char* release = nullptr;

	if (!CallWineHostVersion(getHost, system, release) || system == nullptr)
		return;

	sprintf_s(g_host, "%.40s %.40s", system, release != nullptr ? release : "");
}

void DetectWine()
{
	const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll == nullptr)
		return;

	const auto getVersion = reinterpret_cast<WineGetVersion_t>(
		GetProcAddress(ntdll, "wine_get_version"));

	if (getVersion == nullptr)
		return;

	g_isWine = true;

	const char* version = CallWineVersion(getVersion);
	if (version != nullptr)
		strncpy_s(g_wineVersion, version, _TRUNCATE);

	ReadWineHost(ntdll);
}

void DetectRtss()
{
	HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "RTSSSharedMemoryV2");
	if (mapping == nullptr)
		return;

	CloseHandle(mapping);
	g_rtss = true;
}

void BuildDescription()
{
	const char* const rtss = g_rtss ? ", RTSS present" : "";

	if (!g_isWine)
	{
		sprintf_s(g_description, "Windows%s", rtss);
		return;
	}

	sprintf_s(g_description, "Wine/Proton %s%s%s%s",
		g_wineVersion[0] != '\0' ? g_wineVersion : "(version withheld)",
		g_host[0] != '\0' ? " on " : "", g_host, rtss);
}

}

void Compat::Detect()
{
	if (g_detected)
		return;

	g_detected = true;

	DetectWine();
	DetectRtss();
	BuildDescription();

	LOG("Environment: %s", g_description);

	if (!g_rtss)
		return;

	LOG("RTSS is in this process. Hooks will be installed at the end of the jump chain so its own "
		"hooks are never overwritten.");
}

bool Compat::IsWine()
{
	return g_isWine;
}

const char* Compat::WineVersion()
{
	return g_wineVersion;
}

const char* Compat::HostSystem()
{
	return g_host;
}

bool Compat::IsRtssPresent()
{
	return g_rtss;
}

bool Compat::SafeMode()
{
	if (g_modVals.wineSafeMode < 0)
		return g_isWine;

	return g_modVals.wineSafeMode != 0;
}

void Compat::StandDown()
{
	g_stoodDown = true;
}

bool Compat::StoodDown()
{
	return g_stoodDown;
}

const char* Compat::Describe()
{
	return g_description;
}
