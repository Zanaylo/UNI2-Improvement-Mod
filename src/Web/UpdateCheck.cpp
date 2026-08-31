#include "Web/UpdateCheck.h"

#include "Core/info.h"
#include "Core/interfaces.h"
#include "Core/logger.h"

#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>

namespace {

constexpr int kNotesLength = 2048;

std::mutex g_lock;
GitHubRelease::Release g_release;

std::atomic<bool> g_checking{ false };
std::atomic<bool> g_answered{ false };
std::atomic<bool> g_newer{ false };
std::atomic<bool> g_dismissed{ false };

char g_version[32] = {};
char g_page[256] = UNI2_IM_RELEASE_PAGE;
char g_notes[kNotesLength] = {};
char g_status[256] = "not checked yet";

void Publish(const GitHubRelease::Release& release, bool newer)
{
	std::lock_guard<std::mutex> guard(g_lock);

	g_release = release;

	strncpy_s(g_version, release.version.c_str(), _TRUNCATE);
	strncpy_s(g_page, release.page.empty() ? UNI2_IM_RELEASE_PAGE : release.page.c_str(),
		_TRUNCATE);
	strncpy_s(g_notes, release.notes.c_str(), _TRUNCATE);

	if (newer)
		sprintf_s(g_status, "%s is out, this is %s", release.version.c_str(), UNI2_IM_VERSION);
	else
		strncpy_s(g_status, "this is the latest release", _TRUNCATE);

	g_newer.store(newer);
}

void Fail(const char* error)
{
	std::lock_guard<std::mutex> guard(g_lock);

	sprintf_s(g_status, "the check did not answer - %.180s", error);
	g_newer.store(false);
}

DWORD WINAPI Run(LPVOID)
{
	GitHubRelease::Release release;
	std::string error;

	if (!GitHubRelease::FetchLatest(release, error))
	{
		Fail(error.c_str());
		LOG("update check: %s", error.c_str());
	}
	else if (release.draft)
	{
		Fail("the newest release is still a draft");
	}
	else
	{
		const bool newer = GitHubRelease::IsNewerThanRunning(release.version);

		Publish(release, newer);

		LOG("update check: %s is the latest, this is %s%s", release.version.c_str(),
			UNI2_IM_VERSION, newer ? " - an update is available" : "");
	}

	g_answered.store(true);
	g_checking.store(false);
	return 0;
}

void Launch()
{
	if (g_checking.exchange(true))
		return;

	{
		std::lock_guard<std::mutex> guard(g_lock);
		strncpy_s(g_status, "asking GitHub", _TRUNCATE);
	}

	const HANDLE thread = CreateThread(nullptr, 0, &Run, nullptr, 0, nullptr);

	if (thread != nullptr)
	{
		CloseHandle(thread);
		return;
	}

	g_checking.store(false);
	Fail("a thread could not be started");
}

}

void UpdateCheck::Start()
{
	if (!g_modVals.checkForUpdates)
		return;

	Launch();
}

void UpdateCheck::Refresh()
{
	g_dismissed.store(false);
	Launch();
}

bool UpdateCheck::IsChecking()
{
	return g_checking.load();
}

bool UpdateCheck::HasNewer()
{
	return g_newer.load();
}

bool UpdateCheck::HasAnswer()
{
	return g_answered.load();
}

const char* UpdateCheck::GetLatestVersion()
{
	return g_version;
}

const char* UpdateCheck::GetReleaseUrl()
{
	return g_page;
}

const char* UpdateCheck::GetReleaseNotes()
{
	return g_notes;
}

const char* UpdateCheck::GetStatusText()
{
	return g_status;
}

bool UpdateCheck::CopyRelease(GitHubRelease::Release& out)
{
	std::lock_guard<std::mutex> guard(g_lock);

	if (g_release.tag.empty())
		return false;

	out = g_release;
	return true;
}

void UpdateCheck::Dismiss()
{
	g_dismissed.store(true);
}

bool UpdateCheck::WasDismissed()
{
	return g_dismissed.load();
}
