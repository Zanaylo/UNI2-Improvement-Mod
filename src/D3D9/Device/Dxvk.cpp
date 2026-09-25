#include "D3D9/Device/Dxvk.h"

#include "Core/Config/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <cstdio>
#include <cstring>
#include <mutex>

namespace {

constexpr const char* kSection = "DXVK";
constexpr const char* kFolder = "DXVK\\";
constexpr const char* kRuntime = "d3d9.dll";
constexpr const char* const kSources[] = { "x32\\", "" };

std::mutex g_loadLock;
bool g_read = false;
bool g_enabled = false;
bool g_loadTried = false;
bool g_failed = false;
HMODULE g_module = nullptr;

char g_status[320] = "not asked yet";

constexpr const char* kLogFile = "uni2_d3d9.log";
constexpr DWORD kPresentModeRecheckMs = 1000;

char g_presentMode[64] = {};
DWORD g_presentModeCheckedAt = 0;

bool Exists(const std::string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void RefreshPresentMode()
{
	const DWORD now = GetTickCount();

	if (g_presentModeCheckedAt != 0 && now - g_presentModeCheckedAt < kPresentModeRecheckMs)
		return;

	g_presentModeCheckedAt = now;

	FILE* file = nullptr;
	if (fopen_s(&file, (GetModLogPath() + kLogFile).c_str(), "r") != 0 || file == nullptr)
	{
		g_presentMode[0] = '\0';
		return;
	}

	char found[64] = {};
	char line[512] = {};

	while (fgets(line, sizeof(line), file))
	{
		const char* const marker = strstr(line, "Present mode:");
		if (marker == nullptr)
			continue;

		char mode[64] = {};
		if (sscanf_s(marker, "Present mode: %63s", mode, static_cast<unsigned>(sizeof(mode))) == 1)
			strncpy_s(found, mode, _TRUNCATE);
	}

	fclose(file);
	strncpy_s(g_presentMode, found, _TRUNCATE);
}

void Read()
{
	if (g_read)
		return;

	g_enabled = GetPrivateProfileIntA(kSection, "Enabled", 0, Settings::GetIniPath().c_str()) != 0;
	g_read = true;
}

std::string SourceIn(const std::string& picked)
{
	std::string root = picked;

	if (!root.empty() && root.back() != '\\')
		root += '\\';

	for (const char* const sub : kSources)
	{
		const std::string candidate = root + sub;

		if (Exists(candidate + kRuntime))
			return candidate;
	}

	return std::string();
}

}

HMODULE Dxvk::Load()
{
	std::lock_guard<std::mutex> lock(g_loadLock);

	if (g_loadTried)
		return g_module;

	g_loadTried = true;

	Read();

	if (!g_enabled)
		return nullptr;

	const std::string runtime = Folder() + kRuntime;

	if (!Exists(runtime))
	{
		g_failed = true;
		sprintf_s(g_status, "On, but %s is missing. Pick the DXVK folder again.", runtime.c_str());
		LOG("DXVK: %s", g_status);
		return nullptr;
	}

	SetEnvironmentVariableA("DXVK_LOG_PATH", GetModLogPath().c_str());

	g_module = LoadLibraryA(runtime.c_str());

	if (g_module == nullptr)
	{
		g_failed = true;
		sprintf_s(g_status, "%s failed to load (error %lu), so the game uses the normal Direct3D 9",
			runtime.c_str(), GetLastError());
		LOG("DXVK: %s", g_status);
		return nullptr;
	}

	sprintf_s(g_status, "Running: the game draws through %s", runtime.c_str());
	LOG("DXVK: %s", g_status);

	return g_module;
}

bool Dxvk::IsInstalled()
{
	return Exists(Folder() + kRuntime);
}

bool Dxvk::IsRunning()
{
	return g_module != nullptr;
}

bool Dxvk::IsEnabled()
{
	Read();
	return g_enabled;
}

void Dxvk::SetEnabled(bool enabled)
{
	Read();
	g_enabled = enabled;
	Settings::SaveInt(kSection, "Enabled", g_enabled ? 1 : 0);
}

bool Dxvk::InstallFrom(const std::string& folder, char* status, int statusSize)
{
	const std::string source = SourceIn(folder);

	if (source.empty())
	{
		sprintf_s(status, statusSize, "No %s in that folder or in its x32 folder", kRuntime);
		return false;
	}

	const std::string target = Folder();

	CreateDirectoryA(target.c_str(), nullptr);

	if (!CopyFileA((source + kRuntime).c_str(), (target + kRuntime).c_str(), FALSE))
	{
		sprintf_s(status, statusSize, "Could not copy %s into %s (error %lu)", kRuntime,
			target.c_str(), GetLastError());
		return false;
	}

	sprintf_s(status, statusSize, "%s copied from %s", kRuntime, source.c_str());
	LOG("DXVK: %s", status);

	return true;
}

std::string Dxvk::Folder()
{
	return GetModRootPath(kFolder);
}

const char* Dxvk::StatusText()
{
	Read();

	if (g_module != nullptr || g_failed)
		return g_status;

	if (!IsInstalled())
		return "Not installed. Pick the folder you extracted DXVK into (the one with x32\\d3d9.dll).";

	if (g_enabled)
		return "On. Restart the game to use it.";

	return "Installed but off. The game uses the normal Direct3D 9.";
}

const char* Dxvk::LastPresentMode()
{
	RefreshPresentMode();
	return g_presentMode[0] != '\0' ? g_presentMode : nullptr;
}
