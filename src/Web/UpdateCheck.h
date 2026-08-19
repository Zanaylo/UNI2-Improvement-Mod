#pragma once

namespace UpdateCheck
{
	void Start();

	bool HasNewer();

	const char* GetLatestVersion();
	const char* GetReleaseUrl();

	void Dismiss();
	bool WasDismissed();
}
