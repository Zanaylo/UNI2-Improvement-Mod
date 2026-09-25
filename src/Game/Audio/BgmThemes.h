#pragma once

namespace BgmThemes
{
	constexpr int kMaxThemes = 16;
	constexpr int kMaxEntries = 64;

	struct Theme
	{
		char id[32];
		char name[64];
		char author[64];
		char notes[160];
		int entryCount;
		int readyCount;
	};

	void Reload();

	int Count();
	const Theme* Get(int index);

	int ActiveIndex();

	bool Apply(int index);
	void Clear();

	const char* ThemesPath();
}
