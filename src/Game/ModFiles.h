#pragma once

namespace ModFiles
{
	bool Initialize();

	void SetThemeFolder(const char* folder);

	int Count();
	int Hits();
	int ThemeCount();

	const char* Root();
	const char* StatusText();
}
