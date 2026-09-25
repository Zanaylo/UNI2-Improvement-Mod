#pragma once

namespace BgmControl
{
	bool Initialize();
	bool IsHooked();

	bool Play(int id);
	void Stop();
	void Release();

	void OnFrame();
	void Reshuffle();

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
