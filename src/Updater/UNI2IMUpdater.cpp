#include <Windows.h>
#include <shellapi.h>

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace {

constexpr const wchar_t* kEntryDll = L"dinput8.dll";
constexpr const wchar_t* kUpdaterExe = L"UNI2IMUpdater.exe";
constexpr const wchar_t* kGameExe = L"uni2.exe";

constexpr DWORD kParentWaitMs = 30000;
constexpr int kUnlockTries = 120;
constexpr DWORD kUnlockPauseMs = 500;

struct Handoff
{
	std::wstring installRoot;
	std::wstring stagedRoot;
	std::wstring backupRoot;
	std::wstring logPath;
	std::wstring gameExe;
	std::wstring steamAppId;
	std::wstring tag;
	DWORD parentPid = 0;
	bool relaunch = true;
};

std::string Narrow(const std::wstring& value)
{
	if (value.empty())
		return std::string();

	const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr,
		nullptr);

	if (length <= 1)
		return std::string();

	std::string text(static_cast<size_t>(length - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &text[0], length, nullptr, nullptr);
	return text;
}

std::wstring Combine(const std::wstring& folder, const std::wstring& name)
{
	if (folder.empty())
		return name;

	if (name.empty())
		return folder;

	if (folder.back() == L'\\' || folder.back() == L'/')
		return folder + name;

	return folder + L"\\" + name;
}

std::wstring Parent(const std::wstring& path)
{
	const size_t slash = path.find_last_of(L"\\/");

	return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

bool Exists(const std::wstring& path)
{
	return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool EnsureFolder(const std::wstring& path)
{
	if (path.empty() || Exists(path))
		return true;

	if (!EnsureFolder(Parent(path)))
		return false;

	return CreateDirectoryW(path.c_str(), nullptr) != FALSE ||
		GetLastError() == ERROR_ALREADY_EXISTS;
}

std::string Stamp()
{
	std::time_t now = std::time(nullptr);
	std::tm parts = {};
	gmtime_s(&parts, &now);

	char text[32] = {};
	std::snprintf(text, sizeof(text), "%04d%02d%02d-%02d%02d%02d", parts.tm_year + 1900,
		parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);

	return text;
}

const Handoff* g_logTarget = nullptr;

void Log(const char* format, ...)
{
	if (g_logTarget == nullptr || g_logTarget->logPath.empty())
		return;

	EnsureFolder(Parent(g_logTarget->logPath));

	FILE* file = nullptr;

	if (_wfopen_s(&file, g_logTarget->logPath.c_str(), L"ab") != 0 || file == nullptr)
		return;

	const std::string prefix = "[" + Stamp() + "] ";
	fwrite(prefix.data(), 1, prefix.size(), file);

	va_list args;
	va_start(args, format);
	vfprintf(file, format, args);
	va_end(args);

	fwrite("\r\n", 1, 2, file);
	fclose(file);
}

std::wstring ReadIni(const std::wstring& path, const wchar_t* key)
{
	wchar_t buffer[1024] = {};

	GetPrivateProfileStringW(L"Update", key, L"", buffer, 1024, path.c_str());

	return buffer;
}

bool LoadHandoff(const std::wstring& path, Handoff& out)
{
	out.installRoot = ReadIni(path, L"InstallRoot");
	out.stagedRoot = ReadIni(path, L"StagedRoot");
	out.backupRoot = ReadIni(path, L"BackupRoot");
	out.logPath = ReadIni(path, L"LogPath");
	out.gameExe = ReadIni(path, L"GameExe");
	out.steamAppId = ReadIni(path, L"SteamAppId");
	out.tag = ReadIni(path, L"Tag");
	out.parentPid = GetPrivateProfileIntW(L"Update", L"ParentPid", 0, path.c_str());
	out.relaunch = GetPrivateProfileIntW(L"Update", L"Relaunch", 1, path.c_str()) != 0;

	return !out.installRoot.empty() && !out.stagedRoot.empty() && !out.backupRoot.empty() &&
		!out.tag.empty();
}

bool CopyInto(const std::wstring& from, const std::wstring& to)
{
	EnsureFolder(Parent(to));

	return CopyFileW(from.c_str(), to.c_str(), FALSE) != FALSE;
}

void WaitForParent(const Handoff& handoff)
{
	if (handoff.parentPid == 0)
		return;

	const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, handoff.parentPid);

	if (process == nullptr)
		return;

	WaitForSingleObject(process, kParentWaitMs);
	CloseHandle(process);
}

bool WaitForUnlock(const std::wstring& path)
{
	for (int i = 0; i < kUnlockTries; ++i)
	{
		const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(file);
			return true;
		}

		if (GetLastError() == ERROR_FILE_NOT_FOUND)
			return true;

		Sleep(kUnlockPauseMs);
	}

	return false;
}

bool BackUp(const Handoff& handoff, const std::wstring& folder, const std::wstring& name,
	std::vector<std::wstring>& touched)
{
	const std::wstring source = Combine(handoff.installRoot, name);

	if (!Exists(source))
		return true;

	if (!CopyInto(source, Combine(folder, name)))
	{
		Log("backup failed for %s (Windows error %lu)", Narrow(name).c_str(), GetLastError());
		return false;
	}

	touched.push_back(name);
	return true;
}

void Rollback(const Handoff& handoff, const std::wstring& folder,
	const std::vector<std::wstring>& touched)
{
	for (const std::wstring& name : touched)
	{
		const std::wstring source = Combine(folder, name);

		if (Exists(source))
			CopyInto(source, Combine(handoff.installRoot, name));
	}

	Log("rolled %d file(s) back", static_cast<int>(touched.size()));
}

bool Apply(const Handoff& handoff)
{
	if (!Exists(Combine(handoff.installRoot, kGameExe)))
	{
		Log("install root rejected: %s is not there", Narrow(kGameExe).c_str());
		return false;
	}

	WaitForParent(handoff);

	if (!WaitForUnlock(Combine(handoff.installRoot, kEntryDll)))
	{
		Log("timed out waiting for %s to unlock", Narrow(kEntryDll).c_str());
		return false;
	}

	const std::string stamp = Stamp();
	const std::wstring folder = Combine(handoff.backupRoot,
		handoff.tag + L"-" + std::wstring(stamp.begin(), stamp.end()));

	if (!EnsureFolder(folder))
	{
		Log("backup folder could not be created (Windows error %lu)", GetLastError());
		return false;
	}

	const std::wstring files[] = { kEntryDll, kUpdaterExe };
	std::vector<std::wstring> touched;

	for (const std::wstring& name : files)
	{
		if (!BackUp(handoff, folder, name, touched))
			return false;
	}

	for (const std::wstring& name : files)
	{
		const std::wstring source = Combine(handoff.stagedRoot, name);

		if (!Exists(source))
			continue;

		if (CopyInto(source, Combine(handoff.installRoot, name)))
			continue;

		Log("copy failed for %s (Windows error %lu)", Narrow(name).c_str(), GetLastError());
		Rollback(handoff, folder, touched);
		return false;
	}

	Log("%s applied", Narrow(handoff.tag).c_str());
	return true;
}

void Relaunch(const Handoff& handoff)
{
	if (!handoff.relaunch)
		return;

	if (!handoff.steamAppId.empty())
	{
		const std::wstring url = L"steam://rungameid/" + handoff.steamAppId;

		if (reinterpret_cast<intptr_t>(ShellExecuteW(nullptr, L"open", url.c_str(), nullptr,
			nullptr, SW_SHOWNORMAL)) > 32)
		{
			return;
		}
	}

	if (handoff.gameExe.empty())
		return;

	ShellExecuteW(nullptr, L"open", handoff.gameExe.c_str(), nullptr,
		handoff.installRoot.c_str(), SW_SHOWNORMAL);
}

std::wstring HandoffFromCommandLine()
{
	int count = 0;
	LPWSTR* const parts = CommandLineToArgvW(GetCommandLineW(), &count);

	std::wstring path;

	for (int i = 1; parts != nullptr && i + 1 < count; ++i)
	{
		if (std::wstring(parts[i]) == L"--handoff")
			path = parts[i + 1];
	}

	if (parts != nullptr)
		LocalFree(parts);

	return path;
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	const std::wstring path = HandoffFromCommandLine();

	Handoff handoff;

	if (path.empty() || !LoadHandoff(path, handoff))
		return 2;

	g_logTarget = &handoff;

	Log("updater started for %s", Narrow(handoff.tag).c_str());

	if (!Apply(handoff))
	{
		MessageBoxW(nullptr, L"The UNI2 Improvement Mod update could not be applied. "
			L"UNI2-IM\\Updater\\logs\\updater.log says why, and the previous files are still in "
			L"place.", L"UNI2 Improvement Mod", MB_ICONERROR | MB_OK);

		return 1;
	}

	Relaunch(handoff);
	return 0;
}
