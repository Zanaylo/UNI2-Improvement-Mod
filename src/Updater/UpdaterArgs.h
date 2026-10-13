#pragma once

#include <Windows.h>

#include <string>
#include <vector>

namespace UpdaterArgs
{
	enum class Mode
	{
		Standalone,
		Handoff,
		GameUpdate,
	};

	struct Request
	{
		Mode mode = Mode::Standalone;
		std::wstring handoffPath;
		DWORD parentPid = 0;
	};

	Request Parse(const std::vector<std::wstring>& arguments);
	Request FromCommandLine();
}
