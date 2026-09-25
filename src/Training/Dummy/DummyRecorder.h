#pragma once

#include <cstdint>

namespace DummyRecorder
{
	constexpr int kMaxCallSites = 6;

	bool Install();
	bool IsInstalled();

	void Update();

	uint32_t GetState();
	uint32_t GetFieldB();
	uint32_t GetFieldC();
	uint32_t GetActionSetting();

	uint32_t GetReversalMove(int index);
	uint32_t GetReversalEnabled(int index);

	constexpr int kForcedSlots = 2;
	void SetForced(int slot, uintptr_t rva, uint32_t value);
	bool GetForced(int slot, uintptr_t& outRva, uint32_t& outValue, uint32_t& outCurrent);
	uint32_t GetActionMode();
	int GetReversalHoldRemaining();
	uint64_t GetRestartCount();

	uint64_t GetCallCount();
	uint64_t GetDeferredCount();

	int GetLeadInRemaining();
	int GetLeadInLength();

	constexpr int kMaxChanges = 64;

	void SetFrameCounterRva(uintptr_t rva);
	uintptr_t GetFrameCounterRva();

	void SnapshotRegion();
	void RefreshChanges();
	bool HasSnapshot();
	int GetChangeCount();

	bool GetChange(int index, uintptr_t& outRva, uint32_t& outBefore, uint32_t& outAfter,
		int& outLength);

	int GetCallSiteCount();
	bool GetCallSite(int index, uintptr_t& outReturnRva, uint64_t& outCalls);

	constexpr int kHistory = 8;
	int GetTransitionCount();
	bool GetTransition(int index, uint32_t& outFrom, uint32_t& outTo);
}
