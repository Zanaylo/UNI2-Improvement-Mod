#include "Game/MemoryScanner.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>
#include <cstring>
#include <vector>

namespace {

constexpr size_t kMaxCandidates = 4 * 1000 * 1000;

struct Candidate
{
	uintptr_t address;
	uint32_t previous;
};

std::vector<Candidate> g_candidates;
bool g_seeded = false;
size_t g_scannedSlots = 0;

bool IsReadableRegion(const MEMORY_BASIC_INFORMATION& mbi)
{
	if (mbi.State != MEM_COMMIT)
		return false;

	if ((mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
		return false;

	const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE |
		PAGE_EXECUTE_WRITECOPY;

	return (mbi.Protect & writable) != 0;
}

template <typename Visitor>
void ForEachRegion(Visitor&& visit)
{
	SYSTEM_INFO info = {};
	GetSystemInfo(&info);

	uintptr_t address = reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress);
	const uintptr_t end = reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress);

	std::vector<uint8_t> buffer;

	while (address < end)
	{
		MEMORY_BASIC_INFORMATION mbi = {};
		if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) == 0)
			break;

		const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
		const size_t size = mbi.RegionSize;

		if (IsReadableRegion(mbi))
		{

			constexpr size_t kChunk = 4 * 1024 * 1024;

			if (buffer.size() < kChunk)
				buffer.resize(kChunk);

			for (size_t done = 0; done < size; done += kChunk)
			{
				const size_t take = size - done < kChunk ? size - done : kChunk;
				const uintptr_t at = base + done;

				if (!TryReadMemory(buffer.data(), reinterpret_cast<const void*>(at), take))
					continue;

				visit(at, reinterpret_cast<const uint32_t*>(buffer.data()), take / sizeof(uint32_t));
			}
		}

		if (size == 0)
			break;

		address = base + size;
	}
}

}

int MemoryScanner::FindBytes(const uint8_t* pattern, size_t length, uintptr_t* out, int maxHits)
{
	if (pattern == nullptr || length == 0 || out == nullptr || maxHits <= 0)
		return 0;

	int found = 0;

	ForEachRegion([&](uintptr_t base, const uint32_t* values, size_t count)
	{
		if (found >= maxHits)
			return;

		const uint8_t* bytes = reinterpret_cast<const uint8_t*>(values);
		const size_t size = count * sizeof(uint32_t);
		if (size < length)
			return;

		const uint8_t* at = bytes;
		size_t left = size - length + 1;

		while (left > 0 && found < maxHits)
		{
			const uint8_t* hit = static_cast<const uint8_t*>(memchr(at, pattern[0], left));
			if (hit == nullptr)
				break;

			if (memcmp(hit, pattern, length) == 0)
				out[found++] = base + (hit - bytes);

			left -= (hit - at) + 1;
			at = hit + 1;
		}
	});

	LOG("MemoryScanner: %d address(es) hold the %zu byte pattern", found, length);
	return found;
}

bool MemoryScanner::Snapshot()
{
	g_candidates.clear();
	g_scannedSlots = 0;

	ForEachRegion([](uintptr_t base, const uint32_t* values, size_t count)
	{
		g_scannedSlots += count;

		for (size_t i = 0; i < count && g_candidates.size() < kMaxCandidates; ++i)
			g_candidates.push_back({ base + i * sizeof(uint32_t), values[i] });
	});

	g_seeded = true;
	LOG("MemoryScanner: seeded %zu candidates from %zu slots", g_candidates.size(), g_scannedSlots);
	return !g_candidates.empty();
}

bool MemoryScanner::FilterByValue(int32_t value)
{
	if (!g_seeded)
	{
		g_candidates.clear();
		g_scannedSlots = 0;

		const uint32_t target = static_cast<uint32_t>(value);
		ForEachRegion([target](uintptr_t base, const uint32_t* values, size_t count)
		{
			g_scannedSlots += count;

			for (size_t i = 0; i < count && g_candidates.size() < kMaxCandidates; ++i)
			{
				if (values[i] == target)
					g_candidates.push_back({ base + i * sizeof(uint32_t), values[i] });
			}
		});

		g_seeded = true;
		LOG("MemoryScanner: value %d matched %zu of %zu slots", value, g_candidates.size(),
			g_scannedSlots);
		return true;
	}

	size_t kept = 0;
	for (Candidate& candidate : g_candidates)
	{
		uint32_t current = 0;
		if (!TryReadDword(reinterpret_cast<const void*>(candidate.address), current))
			continue;

		if (static_cast<int32_t>(current) != value)
			continue;

		candidate.previous = current;
		g_candidates[kept++] = candidate;
	}

	g_candidates.resize(kept);
	LOG("MemoryScanner: value %d left %zu candidates", value, g_candidates.size());
	return true;
}

bool MemoryScanner::ApplyFilter(FilterMode mode)
{
	if (!g_seeded)
		return false;

	size_t kept = 0;
	for (Candidate& candidate : g_candidates)
	{
		uint32_t value = 0;
		if (!TryReadDword(reinterpret_cast<const void*>(candidate.address), value))
			continue;

		const int32_t current = static_cast<int32_t>(value);
		const int32_t previous = static_cast<int32_t>(candidate.previous);

		bool keep = false;
		switch (mode)
		{
		case Filter_Changed:   keep = current != previous; break;
		case Filter_Unchanged: keep = current == previous; break;
		case Filter_Increased: keep = current > previous; break;
		case Filter_Decreased: keep = current < previous; break;
		}

		if (!keep)
			continue;

		candidate.previous = value;
		g_candidates[kept++] = candidate;
	}

	g_candidates.resize(kept);
	LOG("MemoryScanner: filter %d left %zu candidates", static_cast<int>(mode), g_candidates.size());
	return true;
}

void MemoryScanner::Reset()
{
	g_candidates.clear();
	g_candidates.shrink_to_fit();
	g_scannedSlots = 0;
	g_seeded = false;
}

bool MemoryScanner::IsReady()
{
	return g_seeded;
}

int MemoryScanner::GetTotalSlots()
{
	return static_cast<int>(g_scannedSlots);
}

int MemoryScanner::GetCandidateCount()
{
	return static_cast<int>(g_candidates.size());
}

bool MemoryScanner::IsCandidateInModule(int candidateIndex)
{
	if (candidateIndex < 0 || static_cast<size_t>(candidateIndex) >= g_candidates.size())
		return false;

	return IsAddressInGameModule(g_candidates[candidateIndex].address);
}

bool MemoryScanner::GetCandidate(int candidateIndex, uintptr_t& outRva, uint32_t& outValue,
	uint32_t& outPrevious)
{
	if (candidateIndex < 0 || static_cast<size_t>(candidateIndex) >= g_candidates.size())
		return false;

	const Candidate& candidate = g_candidates[candidateIndex];

	outRva = IsAddressInGameModule(candidate.address)
		? candidate.address - GetGameBaseAddress()
		: candidate.address;

	outPrevious = candidate.previous;
	outValue = 0;
	TryReadDword(reinterpret_cast<const void*>(candidate.address), outValue);
	return true;
}

namespace {

struct PointerHit
{
	uintptr_t address;
	uint32_t offset;
	bool inModule;
};

std::vector<PointerHit> g_pointerHits;
constexpr size_t kMaxPointerHits = 4096;

}

int MemoryScanner::FindPointersTo(uintptr_t target, uint32_t maxOffset)
{
	g_pointerHits.clear();

	if (target == 0)
		return 0;

	const uintptr_t low = target >= maxOffset ? target - maxOffset : 0;

	ForEachRegion([target, low](uintptr_t base, const uint32_t* values, size_t count)
	{
		for (size_t i = 0; i < count && g_pointerHits.size() < kMaxPointerHits; ++i)
		{
			const uintptr_t value = values[i];
			if (value < low || value > target)
				continue;

			const uintptr_t address = base + i * sizeof(uint32_t);

			if (address >= low && address <= target)
				continue;

			g_pointerHits.push_back({ address, static_cast<uint32_t>(target - value),
				IsAddressInGameModule(address) });
		}
	});

	for (size_t i = 0, write = 0; i < g_pointerHits.size(); ++i)
	{
		if (!g_pointerHits[i].inModule)
			continue;

		std::swap(g_pointerHits[write], g_pointerHits[i]);
		++write;
	}

	LOG("MemoryScanner: %zu pointers into 0x%08x-0x%08x", g_pointerHits.size(),
		(unsigned)low, (unsigned)target);
	return static_cast<int>(g_pointerHits.size());
}

int MemoryScanner::GetPointerHitCount()
{
	return static_cast<int>(g_pointerHits.size());
}

bool MemoryScanner::GetPointerHit(int index, uintptr_t& outAddress, uint32_t& outOffset,
	bool& outInModule)
{
	if (index < 0 || static_cast<size_t>(index) >= g_pointerHits.size())
		return false;

	const PointerHit& hit = g_pointerHits[index];
	outAddress = hit.inModule ? hit.address - GetGameBaseAddress() : hit.address;
	outOffset = hit.offset;
	outInModule = hit.inModule;
	return true;
}

void MemoryScanner::LogPointerHits(int maxEntries)
{
	const int count = GetPointerHitCount();
	LOG("[pointers] %d hits", count);

	for (int i = 0; i < count && i < maxEntries; ++i)
	{
		uintptr_t address = 0;
		uint32_t offset = 0;
		bool inModule = false;
		if (!GetPointerHit(i, address, offset, inModule))
			break;

		LOG("[pointers]   %-4s 0x%08x  +0x%x%s", inModule ? "rva" : "heap", (unsigned)address,
			offset, inModule ? "   <-- stable" : "");
	}
}

void MemoryScanner::LogCandidates(int maxEntries)
{
	const int count = GetCandidateCount();
	LOG("MemoryScanner: %d candidates", count);

	for (int i = 0; i < count && i < maxEntries; ++i)
	{
		uintptr_t rva = 0;
		uint32_t value = 0;
		uint32_t previous = 0;
		if (!GetCandidate(i, rva, value, previous))
			break;

		LOG("  %s 0x%08x  value %d (0x%08x)  previous %d",
			IsCandidateInModule(i) ? "rva" : "addr", (unsigned)rva, (int)value, value, (int)previous);
	}
}
