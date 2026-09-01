#pragma once

namespace BgmControl
{
	bool Initialize();
	bool IsHooked();

	bool Play(int id);
	void Stop();
	void Release();

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
}
