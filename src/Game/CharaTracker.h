// The chara data stack is empty outside the game's update, so character pointers cannot be read from
// the render thread. This captures them from inside a chara-scoped hook instead.

#pragma once

#include <cstdint>

namespace CharaTracker
{
	constexpr int kMaxEntries = 32;

	struct Entry
	{
		void* object;
		void* charaData;
		void* paramBlock;
		uint32_t hits;
		uint32_t lastSeenTick;
	};

	bool Install();
	bool IsInstalled();

	uint64_t GetCallCount();

	bool IsBattleActive();
	uint32_t GetLastCallTick();
	int GetEntryCount();
	bool GetEntry(int index, Entry& out);
	void Clear();

	bool IsFresh(const Entry& entry, uint32_t maxAgeMs);

	void RequestStackCapture();
	int GetStackFrameCount();
	uintptr_t GetStackFrame(int index);

	bool ReadCurrentCharacter(void*& outObject, void*& outCharacter);
}
