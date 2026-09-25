#pragma once

namespace ModFiles
{
	bool Initialize();

	void Rescan();
	long Revision();

	void OnFrame();

	int Count();
	int OwnCount();
	int Hits();

	const char* Root();
	const char* LoadText();
	const char* StatusText();
}
