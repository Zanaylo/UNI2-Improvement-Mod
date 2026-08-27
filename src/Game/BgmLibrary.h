#pragma once

namespace BgmLibrary
{
	constexpr int kFirstId = 1000;
	constexpr int kMaxTracks = 768;

	struct Track
	{
		char file[32];
		char title[96];
		char tag[16];
		bool loops;
		double loopPos;
		int volume;
		int slot;
	};

	void Load();

	int Count();
	int IdAt(int index);

	const Track* Get(int id);

	int Find(const char* file);

	int TagCount();
	const char* TagAt(int index);

	bool IsLibraryId(int id);

	bool IsPlayable(int id);
	bool Loops(int id);

	int ParseRef(const char* text);
	void FormatRef(int id, char* out, int size);

	int SlotOf(int id);
	bool IsMirroredSlot(int slot);

	int WindowSlot();
	int Bind(int id);
	int BoundId();

	const char* Path();
	const char* StatusText();
}
