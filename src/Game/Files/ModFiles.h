#pragma once

#include <string>

namespace ModFiles
{
	bool Initialize();

	void Rescan();
	long Revision();

	void OnFrame();

	int Count();
	int UserFileCount();
	int Hits();

	bool Find(const char* gamePath, std::string& out);

	const char* Root();
	const char* LoadText();
	const char* StatusText();
}
