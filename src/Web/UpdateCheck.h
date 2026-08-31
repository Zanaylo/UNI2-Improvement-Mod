#pragma once

#include "Web/GitHubRelease.h"

namespace UpdateCheck
{
	void Start();

	void Refresh();

	bool IsChecking();

	bool HasNewer();

	bool HasAnswer();

	const char* GetLatestVersion();
	const char* GetReleaseUrl();
	const char* GetReleaseNotes();
	const char* GetStatusText();

	bool CopyRelease(GitHubRelease::Release& out);

	void Dismiss();
	bool WasDismissed();
}
