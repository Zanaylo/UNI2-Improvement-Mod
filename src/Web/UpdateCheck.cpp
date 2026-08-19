#include "Web/UpdateCheck.h"

#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/logger.h"

#include <Windows.h>
#include <wininet.h>

#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "wininet.lib")

namespace {

constexpr int kParts = 3;
constexpr DWORD kBufferBytes = 4096;

char g_latest[32] = {};
volatile LONG g_newer = 0;
volatile LONG g_dismissed = 0;

std::string Download(const wchar_t* url)
{
	const HINTERNET session = InternetOpenW(L"UNI2-Improvement-Mod",
		INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);

	if (session == nullptr)
		return std::string();

	const HINTERNET address = InternetOpenUrlW(session, url, nullptr, 0,
		INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_NO_UI | INTERNET_FLAG_SECURE, 0);

	if (address == nullptr)
	{
		InternetCloseHandle(session);
		return std::string();
	}

	std::string received;
	char buffer[kBufferBytes] = {};
	DWORD read = 0;

	while (InternetReadFile(address, buffer, kBufferBytes, &read) && read != 0)
		received.append(buffer, read);

	InternetCloseHandle(address);
	InternetCloseHandle(session);

	return received;
}

bool ReadTagName(const std::string& body, char* out, int size)
{
	const size_t at = body.find("\"tag_name\"");

	if (at == std::string::npos)
		return false;

	const size_t open = body.find('"', body.find(':', at));

	if (open == std::string::npos)
		return false;

	const size_t close = body.find('"', open + 1);

	if (close == std::string::npos || close - open <= 1)
		return false;

	std::string tag = body.substr(open + 1, close - open - 1);

	if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V'))
		tag.erase(0, 1);

	strncpy_s(out, size, tag.c_str(), _TRUNCATE);
	return out[0] != '\0';
}

void ReadParts(const char* version, int* out)
{
	for (int i = 0; i < kParts; ++i)
		out[i] = 0;

	sscanf_s(version, "%d.%d.%d", &out[0], &out[1], &out[2]);
}

bool IsNewer(const char* latest, const char* mine)
{
	int theirs[kParts] = {};
	int ours[kParts] = {};

	ReadParts(latest, theirs);
	ReadParts(mine, ours);

	for (int i = 0; i < kParts; ++i)
	{
		if (theirs[i] != ours[i])
			return theirs[i] > ours[i];
	}

	return false;
}

DWORD WINAPI Run(LPVOID)
{
	const std::string body = Download(UNI2_IM_RELEASE_API);

	if (body.empty())
	{
		LOG("update check: nothing came back");
		return 0;
	}

	char latest[sizeof(g_latest)] = {};

	if (!ReadTagName(body, latest, sizeof(latest)))
	{
		LOG("update check: no tag in the answer");
		return 0;
	}

	strncpy_s(g_latest, latest, _TRUNCATE);

	if (!IsNewer(latest, UNI2_IM_VERSION))
	{
		LOG("update check: %s is the latest, this is %s", latest, UNI2_IM_VERSION);
		return 0;
	}

	InterlockedExchange(&g_newer, 1);
	LOG("update check: %s is out, this is %s", latest, UNI2_IM_VERSION);
	return 0;
}

}

void UpdateCheck::Start()
{
	if (!g_modVals.checkForUpdates)
		return;

	const HANDLE thread = CreateThread(nullptr, 0, &Run, nullptr, 0, nullptr);

	if (thread != nullptr)
		CloseHandle(thread);
}

bool UpdateCheck::HasNewer()
{
	return InterlockedCompareExchange(&g_newer, 0, 0) != 0;
}

const char* UpdateCheck::GetLatestVersion()
{
	return g_latest;
}

const char* UpdateCheck::GetReleaseUrl()
{
	return UNI2_IM_RELEASE_PAGE;
}

void UpdateCheck::Dismiss()
{
	InterlockedExchange(&g_dismissed, 1);
}

bool UpdateCheck::WasDismissed()
{
	return InterlockedCompareExchange(&g_dismissed, 0, 0) != 0;
}
