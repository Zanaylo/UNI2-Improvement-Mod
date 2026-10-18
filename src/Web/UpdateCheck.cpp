#include "Web/UpdateCheck.h"

#include "Core/info.h"
#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace {

constexpr const char* kSection = "Mod";
constexpr const char* kSkippedKey = "SkippedUpdate";

std::mutex g_lock;

std::atomic<bool> g_checking{ false };
std::atomic<bool> g_answered{ false };
std::atomic<bool> g_newer{ false };
std::atomic<bool> g_dismissed{ false };
std::atomic<bool> g_skipped{ false };
std::atomic<long> g_revision{ 0 };

char g_version[32] = {};
char g_page[256] = UNI2_IM_RELEASE_PAGE;
char g_status[256] = "not checked yet";
std::vector<GitHubRelease::Release> g_newerReleases;

bool IsSkipped(const std::string& version)
{
	char stored[32] = {};

	Ini::GetString(kSection, kSkippedKey, "", stored, sizeof(stored), Settings::GetIniPath().c_str());

	return stored[0] != '\0' && version == stored;
}

std::vector<GitHubRelease::Release> Stable(const std::vector<GitHubRelease::Release>& releases)
{
	std::vector<GitHubRelease::Release> stable;

	for (const GitHubRelease::Release& release : releases)
	{
		if (!release.draft && !release.prerelease)
			stable.push_back(release);
	}

	std::sort(stable.begin(), stable.end(),
		[](const GitHubRelease::Release& left, const GitHubRelease::Release& right)
		{
			return GitHubRelease::Compare(left.version, right.version) > 0;
		});

	return stable;
}

std::vector<GitHubRelease::Release> NewerThanRunning(const std::vector<GitHubRelease::Release>& stable)
{
	std::vector<GitHubRelease::Release> newer;

	for (const GitHubRelease::Release& release : stable)
	{
		if (GitHubRelease::IsNewerThanRunning(release.version))
			newer.push_back(release);
	}

	return newer;
}

void Publish(const GitHubRelease::Release& latest, const std::vector<GitHubRelease::Release>& newer)
{
	const bool skipped = !newer.empty() && IsSkipped(latest.version);

	std::lock_guard<std::mutex> guard(g_lock);

	strncpy_s(g_version, latest.version.c_str(), _TRUNCATE);
	strncpy_s(g_page, latest.page.empty() ? UNI2_IM_RELEASE_PAGE : latest.page.c_str(), _TRUNCATE);
	g_newerReleases = newer;

	if (newer.empty())
		strncpy_s(g_status, "you have the latest version", _TRUNCATE);
	else
		sprintf_s(g_status, "%s is out, you have %s%s", latest.version.c_str(), UNI2_IM_VERSION,
			skipped ? " (you skipped it)" : "");

	g_skipped.store(skipped);
	g_newer.store(!newer.empty());
	++g_revision;
}

void Fail(const char* error)
{
	std::lock_guard<std::mutex> guard(g_lock);

	sprintf_s(g_status, "could not check for updates: %.180s", error);
	g_newer.store(false);
}

DWORD WINAPI Run(LPVOID)
{
	std::vector<GitHubRelease::Release> releases;
	std::string error;

	if (!GitHubRelease::FetchAll(releases, error))
	{
		Fail(error.c_str());
		LOG("update check: %s", error.c_str());
	}
	else
	{
		const std::vector<GitHubRelease::Release> stable = Stable(releases);

		if (stable.empty())
		{
			Fail("no release has been published yet");
		}
		else
		{
			const std::vector<GitHubRelease::Release> newer = NewerThanRunning(stable);

			Publish(stable.front(), newer);

			LOG("update check: %s is the latest, this is %s, %d newer release(s)",
				stable.front().version.c_str(), UNI2_IM_VERSION, static_cast<int>(newer.size()));
		}
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
		strncpy_s(g_status, "checking GitHub", _TRUNCATE);
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

const char* UpdateCheck::GetStatusText()
{
	return g_status;
}

std::vector<GitHubRelease::Release> UpdateCheck::GetNewerReleases()
{
	std::lock_guard<std::mutex> guard(g_lock);
	return g_newerReleases;
}

long UpdateCheck::Revision()
{
	return g_revision.load();
}

void UpdateCheck::Dismiss()
{
	g_dismissed.store(true);
}

bool UpdateCheck::WasDismissed()
{
	return g_dismissed.load();
}

void UpdateCheck::SkipLatest()
{
	std::string version;

	{
		std::lock_guard<std::mutex> guard(g_lock);
		version = g_version;
	}

	Settings::SaveString(kSection, kSkippedKey, version.c_str());
	g_skipped.store(true);
	g_dismissed.store(true);
}

bool UpdateCheck::IsLatestSkipped()
{
	return g_skipped.load();
}
