#pragma once

#include "Training/Dummy/InputFrame.h"

#include <cstdint>

namespace DummyScript
{
	using Frame = InputFrame;

	constexpr int kMaxFrames = 3600;

	bool Parse(const char* text, char* outError, int errorSize);

	int GetFrameCount();
	const Frame* GetFrames();

	bool WriteToSlot(int slot);

	constexpr int kMaxLibrary = 64;

	void RefreshLibrary();
	int GetLibraryCount();
	const char* GetLibraryLabel(int index);
	const char* GetLibraryName(int index);
	int GetLibraryChara(int index);
	bool LoadFromLibrary(int index, char* outText, int size);
	bool DeleteFromLibrary(int index);

	bool Save(int player, const char* name, const char* text);

	bool ReadFromSlot(int slot, char* outText, int size);

	bool Play(int slot);
	void Stop();
	bool IsPlaying();
	int GetPlaybackFrame();

	const char* GetStatus();
}
