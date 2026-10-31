#pragma once

#include <string>
#include <vector>

namespace BgmControl
{
	bool Initialize();
	bool IsHooked();

	bool Play(int id);
	void Stop();
	void Release();

	void OnFrame();
	void Reshuffle();
	void Remap(const std::vector<std::string>& before, const std::vector<std::string>& after);

	bool IsPinned();
	int PinnedId();

	int Current();
	void RefreshVolume();
	bool IsSuppressed();

	void WriteCrashReport();

	int GetLastRequested();
	int GetLastPlayed();

	int GetCharacter(int side);

	const char* GetStatusText();
	const char* ReasonText();
}
