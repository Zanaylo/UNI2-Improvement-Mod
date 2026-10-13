#pragma once

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

	void Dismiss();
	bool WasDismissed();
}
