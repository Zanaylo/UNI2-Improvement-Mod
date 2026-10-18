#pragma once

#include "Web/GitHubRelease.h"

#include <vector>

namespace UpdateCheck
{
	void Start();

	void Refresh();

	bool IsChecking();

	bool HasNewer();

	bool HasAnswer();

	const char* GetLatestVersion();
	const char* GetReleaseUrl();
	const char* GetStatusText();

	std::vector<GitHubRelease::Release> GetNewerReleases();
	long Revision();

	void Dismiss();
	bool WasDismissed();

	void SkipLatest();
	bool IsLatestSkipped();
}
