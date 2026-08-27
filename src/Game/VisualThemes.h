#pragma once

namespace VisualThemes
{
	constexpr int kMaxThemes = 16;

	struct Theme
	{
		char id[32];
		char name[64];
		char author[64];
		char notes[192];
		int fileCount;
	};

	void Initialize();
	void Reload();
	void OnFrame();

	int Count();
	const Theme* Get(int index);

	int ActiveIndex();

	bool Apply(int index);
	void Clear();

	bool WasRolledBack();

	const char* Path();
}
