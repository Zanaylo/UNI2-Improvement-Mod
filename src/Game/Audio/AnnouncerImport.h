#pragma once

#include <string>

namespace AnnouncerImport
{
	std::string FindGame();
	bool IsGame(const char* folder);

	bool Begin(const char* folder);

	void Update();

	bool IsBusy();
	int Progress();

	std::string StatusText();
	std::string PackFolder();
}
