#pragma once

#include <Windows.h>

#include <string>

struct Handoff
{
	std::wstring installRoot;
	std::wstring stagedRoot;
	std::wstring backupRoot;
	std::wstring logPath;
	std::wstring gameExe;
	std::wstring steamAppId;
	std::wstring tag;
	DWORD parentPid = 0;
	bool relaunch = true;
};

namespace Install
{
	bool Load(const std::wstring& path, Handoff& out);

	bool IsLocked(const std::wstring& path);

	bool Apply(const Handoff& handoff);
	void Relaunch(const Handoff& handoff);
}
