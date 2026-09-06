#pragma once

namespace ModFiles
{
	bool Initialize();

	void Rescan();

	void OnFrame();

	int Count();
	int OwnCount();
	int Hits();

	const char* Root();
	const char* StatusText();
}
