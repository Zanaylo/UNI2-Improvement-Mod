#pragma once

namespace ModFiles
{
	bool Initialize();

	void Rescan();

	int Count();
	int Hits();

	const char* Root();
	const char* StatusText();
}
