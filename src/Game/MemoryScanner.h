// Cheat-Engine style search over every committed readable region, not just .data - the training
// objects this needs to find are allocated outside it.

#pragma once

#include <cstdint>

namespace MemoryScanner
{
	enum FilterMode
	{
		Filter_Changed,
		Filter_Unchanged,
		Filter_Increased,
		Filter_Decreased
	};

	bool Snapshot();

	bool FilterByValue(int32_t value);

	bool ApplyFilter(FilterMode mode);
	void Reset();

	bool IsReady();
	int GetCandidateCount();
	int GetTotalSlots();

	bool GetCandidate(int candidateIndex, uintptr_t& outRva, uint32_t& outValue, uint32_t& outPrevious);
	bool IsCandidateInModule(int candidateIndex);
	void LogCandidates(int maxEntries);

	int FindBytes(const uint8_t* pattern, size_t length, uintptr_t* out, int maxHits);

	int FindPointersTo(uintptr_t target, uint32_t maxOffset);
	int GetPointerHitCount();
	bool GetPointerHit(int index, uintptr_t& outAddress, uint32_t& outOffset, bool& outInModule);
	void LogPointerHits(int maxEntries);
}
