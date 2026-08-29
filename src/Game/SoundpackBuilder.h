#pragma once

namespace SoundpackBuilder
{
	constexpr int kMaxTracks = 64;

	bool IsOpen();

	void Begin();
	void Cancel();

	int Count();
	int TrackAt(int index);
	int SceneAt(int index);
	void SetScene(int index, int scene);
	void RemoveAt(int index);

	bool Holds(int bgm);
	bool Toggle(int bgm);

	char* NameBuffer();
	int NameCapacity();
	char* AuthorBuffer();
	int AuthorCapacity();

	bool Save(char* status, int statusSize);
}
