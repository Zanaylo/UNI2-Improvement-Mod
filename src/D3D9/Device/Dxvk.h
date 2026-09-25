#pragma once

#include <Windows.h>

#include <string>

namespace Dxvk
{
	HMODULE Load();

	bool IsInstalled();
	bool IsRunning();

	bool IsEnabled();
	void SetEnabled(bool enabled);

	bool InstallFrom(const std::string& folder, char* status, int statusSize);

	std::string Folder();
	const char* StatusText();

	const char* LastPresentMode();
}
