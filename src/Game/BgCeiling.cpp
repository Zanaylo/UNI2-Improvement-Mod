#include "Game/BgCeiling.h"

#include "Core/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr const char* kSection = "Stages";
constexpr const char* kKey = "WidenStageTable";

constexpr size_t kSlotBytes = sizeof(uint32_t);
constexpr uint8_t kInt3 = 0xcc;
constexpr uint8_t kNop = 0x90;
constexpr uint8_t kJumpNear = 0xe9;
constexpr uint8_t kCompareGroup = 0x83;
constexpr uint8_t kCompareWide = 0x81;
constexpr uint8_t kTwoByte = 0x0f;
constexpr uint8_t kNearCondition = 0x80;
constexpr uint8_t kFirstRegisterForm = 0xf8;
constexpr uint8_t kFirstShortCondition = 0x7c;
constexpr uint8_t kLastShortCondition = 0x7f;
constexpr uint8_t kFirstNearCondition = 0x8c;
constexpr uint8_t kLastNearCondition = 0x8f;
constexpr uint8_t kBelowCount = 0x63;
constexpr uint8_t kAtCount = 0x64;
constexpr size_t kCompareBytes = 3;
constexpr size_t kShortJumpBytes = 2;
constexpr size_t kNearJumpBytes = 6;
constexpr size_t kJumpBytes = 5;
constexpr size_t kTrampolineBytes = 20;
constexpr size_t kFunctionWindow = 0x8000;
constexpr int kLeastTableSites = 60;
constexpr int kMostTableSites = 140;
constexpr int kLeastListSites = 4;
constexpr int kMostListSites = 24;
constexpr int kLeastBoundSites = 30;
constexpr int kMostBoundSites = 90;
constexpr int kPaddedNumbers = 0x10000;
constexpr int kPaddedListEntries = 0x1000;
constexpr size_t kRecordBytes = 0x240;
constexpr int kCellSlots = kPaddedListEntries;
constexpr uint8_t kStoreIndexed = 0x89;
constexpr uint8_t kModMask = 0xc7;
constexpr uint8_t kFramedSib = 0x84;
constexpr uint8_t kFramedDisp = 0x85;
constexpr uint8_t kNoBase = 0x05;
constexpr uint8_t kScaleFour = 0x80;
constexpr uint8_t kModClear = 0x3f;
constexpr int32_t kLeastFrameDisp = -0x2000;
constexpr int32_t kMostFrameDisp = -0x100;
constexpr int kCellFunctions = 2;
constexpr int kLeastCellSites = 2;
constexpr int kMostCellSites = 12;

struct Bound
{
	uint8_t* at;
	uint8_t modrm;
	uint8_t condition;
	uint8_t counted;
	size_t bytes;
	uint8_t* target;
};

struct Cell
{
	uint8_t* modrm;
	uint8_t* disp;
};

uint8_t* g_text = nullptr;
size_t g_textBytes = 0;

bool g_lifted = false;
bool g_restacked = false;
uintptr_t g_table = 0;
char g_status[224] = "the game's own hundred stage numbers";

bool TakeText()
{
	const uintptr_t base = GetGameBaseAddress();

	if (base == 0)
		return false;

	const IMAGE_DOS_HEADER* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);

	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return false;

	const IMAGE_NT_HEADERS* const nt =
		reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);

	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return false;

	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);

	for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
	{
		if (memcmp(section->Name, ".text", 6) != 0)
			continue;

		g_text = reinterpret_cast<uint8_t*>(base + section->VirtualAddress);
		g_textBytes = section->Misc.VirtualSize;

		return g_textBytes > kFunctionWindow;
	}

	return false;
}

bool Poke(void* at, const void* bytes, size_t size)
{
	DWORD previous = 0;

	if (VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &previous) == 0)
		return false;

	memcpy(at, bytes, size);
	VirtualProtect(at, size, previous, &previous);
	FlushInstructionCache(GetCurrentProcess(), at, size);

	return true;
}

void Sites(uint32_t wanted, std::vector<uint8_t*>& out)
{
	for (size_t at = 0; at + sizeof(wanted) <= g_textBytes; ++at)
	{
		if (memcmp(g_text + at, &wanted, sizeof(wanted)) == 0)
			out.push_back(g_text + at);
	}
}

bool Holds(const uint8_t* at, const uint32_t* wanted, int count)
{
	const uint8_t* const floor = at - g_text > static_cast<ptrdiff_t>(kFunctionWindow)
		? at - kFunctionWindow : g_text;

	const uint8_t* low = at;

	while (low > floor && !(low[-1] == kInt3 && low[0] != kInt3))
		--low;

	const uint8_t* const ceiling = g_text + g_textBytes - 3;
	const uint8_t* const roof = ceiling - at > static_cast<ptrdiff_t>(kFunctionWindow)
		? at + kFunctionWindow : ceiling;

	const uint8_t* high = at;

	while (high < roof && !(high[0] == kInt3 && high[1] == kInt3 && high[2] == kInt3))
		++high;

	for (const uint8_t* scan = low; scan + sizeof(uint32_t) <= high; ++scan)
	{
		for (int i = 0; i < count; ++i)
		{
			if (memcmp(scan, &wanted[i], sizeof(uint32_t)) == 0)
				return true;
		}
	}

	return false;
}

bool Decoded(uint8_t* at, Bound& out)
{
	if (at[0] != kCompareGroup || at[1] < kFirstRegisterForm)
		return false;

	if (at[2] != kBelowCount && at[2] != kAtCount)
		return false;

	uint8_t* const jump = at + kCompareBytes;

	if (jump[0] >= kFirstShortCondition && jump[0] <= kLastShortCondition)
	{
		out.condition = static_cast<uint8_t>(jump[0] & 0x0f);
		out.bytes = kShortJumpBytes;
		out.target = jump + kShortJumpBytes + static_cast<int8_t>(jump[1]);
	}
	else if (jump[0] == kTwoByte && jump[1] >= kFirstNearCondition &&
		jump[1] <= kLastNearCondition)
	{
		int32_t relative = 0;
		memcpy(&relative, jump + 2, sizeof(relative));

		out.condition = static_cast<uint8_t>(jump[1] & 0x0f);
		out.bytes = kNearJumpBytes;
		out.target = jump + kNearJumpBytes + relative;
	}
	else
	{
		return false;
	}

	out.at = at;
	out.modrm = at[1];
	out.counted = at[2];

	return true;
}

void Bounds(const uint32_t* arrays, int count, std::vector<Bound>& out)
{
	for (size_t at = 0; at + kCompareBytes + kNearJumpBytes <= g_textBytes; ++at)
	{
		Bound bound = {};

		if (!Decoded(g_text + at, bound) || !Holds(g_text + at, arrays, count))
			continue;

		out.push_back(bound);
	}
}

bool Emit(const Bound& bound, uint8_t* trampoline, int ceiling)
{
	const int32_t wanted = bound.counted == kAtCount ? ceiling : ceiling - 1;

	uint8_t body[kTrampolineBytes] = {};
	size_t at = 0;

	body[at++] = kCompareWide;
	body[at++] = bound.modrm;
	memcpy(body + at, &wanted, sizeof(wanted));
	at += sizeof(wanted);

	body[at++] = kTwoByte;
	body[at++] = static_cast<uint8_t>(kNearCondition | bound.condition);

	const int32_t toTarget = static_cast<int32_t>(bound.target - (trampoline + at + 4));
	memcpy(body + at, &toTarget, sizeof(toTarget));
	at += sizeof(toTarget);

	uint8_t* const resume = bound.at + kCompareBytes + bound.bytes;

	body[at++] = kJumpNear;

	const int32_t toResume = static_cast<int32_t>(resume - (trampoline + at + 4));
	memcpy(body + at, &toResume, sizeof(toResume));
	at += sizeof(toResume);

	memcpy(trampoline, body, kTrampolineBytes);

	uint8_t patch[kCompareBytes + kNearJumpBytes];
	memset(patch, kNop, sizeof(patch));
	patch[0] = kJumpNear;

	const int32_t toTrampoline = static_cast<int32_t>(trampoline - (bound.at + kJumpBytes));
	memcpy(patch + 1, &toTrampoline, sizeof(toTrampoline));

	return Poke(bound.at, patch, kCompareBytes + bound.bytes);
}

uint8_t* Widened(uintptr_t stock, int entries, int padded, uint32_t beyond)
{
	uint8_t* const wide = static_cast<uint8_t*>(VirtualAlloc(nullptr,
		static_cast<size_t>(padded) * kSlotBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

	if (wide == nullptr)
		return nullptr;

	memcpy(wide, reinterpret_cast<const void*>(stock),
		static_cast<size_t>(BgCeiling::kStockNumbers) * kSlotBytes);

	if (beyond == 0)
		return wide;

	uint32_t* const slot = reinterpret_cast<uint32_t*>(wide);

	for (int i = entries; i < padded; ++i)
		slot[i] = beyond;

	return wide;
}

bool Framed(const uint8_t* at, int32_t disp)
{
	int32_t held = 0;
	memcpy(&held, at, sizeof(held));

	return held == disp;
}

bool Cellular(uint8_t* disp, Cell& out)
{
	uint8_t* const sib = disp - 1;

	if ((sib[-1] & kModMask) == kFramedSib && (sib[0] & 0x07) == kNoBase)
	{
		out.modrm = sib - 1;
		out.disp = disp;
		return true;
	}

	if ((sib[0] & kModMask) == kFramedDisp)
	{
		out.modrm = sib;
		out.disp = disp;
		return true;
	}

	return false;
}

bool CellArray(uint8_t* low, uint8_t* high, int32_t& disp)
{
	for (uint8_t* at = low; at + 7 <= high; ++at)
	{
		if (at[0] != kStoreIndexed || (at[1] & kModMask) != kFramedSib)
			continue;

		if ((at[2] & 0x07) != kNoBase || (at[2] & 0xc0) != kScaleFour)
			continue;

		int32_t held = 0;
		memcpy(&held, at + 3, sizeof(held));

		if (held < kLeastFrameDisp || held > kMostFrameDisp)
			continue;

		disp = held;
		return true;
	}

	return false;
}

void Extent(uint8_t* at, uint8_t*& low, uint8_t*& high)
{
	const uint8_t* const floor = at - g_text > static_cast<ptrdiff_t>(kFunctionWindow)
		? at - kFunctionWindow : g_text;

	low = at;

	while (low > floor && !(low[-1] == kInt3 && low[0] != kInt3))
		--low;

	uint8_t* const ceiling = g_text + g_textBytes - 3;
	const uint8_t* const roof = ceiling - at > static_cast<ptrdiff_t>(kFunctionWindow)
		? at + kFunctionWindow : ceiling;

	high = at;

	while (high < roof && !(high[0] == kInt3 && high[1] == kInt3 && high[2] == kInt3))
		++high;
}

void Cells(const std::vector<uint8_t*>& listSites, std::vector<std::vector<Cell> >& out)
{
	std::vector<uint8_t*> seen;

	for (uint8_t* const site : listSites)
	{
		uint8_t* low = nullptr;
		uint8_t* high = nullptr;
		Extent(site, low, high);

		bool already = false;

		for (uint8_t* const held : seen)
			already = already || held == low;

		if (already)
			continue;

		int32_t disp = 0;

		if (!CellArray(low, high, disp))
			continue;

		seen.push_back(low);

		std::vector<Cell> found;

		for (uint8_t* at = low + 2; at + sizeof(disp) <= high; ++at)
		{
			Cell cell = {};

			if (Framed(at, disp) && Cellular(at, cell))
				found.push_back(cell);
		}

		out.push_back(found);
	}
}

bool Restack(const std::vector<Cell>& cells)
{
	uint8_t* const buffer = static_cast<uint8_t*>(VirtualAlloc(nullptr,
		static_cast<size_t>(kCellSlots) * kSlotBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

	if (buffer == nullptr)
		return false;

	const uint32_t now = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer));

	for (const Cell& cell : cells)
	{
		const uint8_t modrm = static_cast<uint8_t>(cell.modrm[0] & kModClear);

		if (!Poke(cell.modrm, &modrm, sizeof(modrm)) || !Poke(cell.disp, &now, sizeof(now)))
			return false;
	}

	return true;
}

int Repoint(const std::vector<uint8_t*>& sites, const uint8_t* wide)
{
	const uint32_t now = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wide));
	int done = 0;

	for (uint8_t* const site : sites)
		done += Poke(site, &now, sizeof(now)) ? 1 : 0;

	return done;
}

bool Wanted()
{
	char stored[16] = {};

	GetPrivateProfileStringA(kSection, kKey, "1", stored, sizeof(stored),
		Settings::GetIniPath().c_str());

	return stored[0] != '0';
}

}

bool BgCeiling::Initialize()
{
	if (!Wanted())
	{
		strncpy_s(g_status, "left at the game's own hundred, by request", _TRUNCATE);
		LOG("BgCeiling: %s", g_status);
		return false;
	}

	if (!TakeText())
	{
		strncpy_s(g_status, "the game's code section could not be read", _TRUNCATE);
		LOG("BgCeiling: %s", g_status);
		return false;
	}

	const uintptr_t table = RvaToAddress(GameOffsets::kBgRecordTable);
	const uintptr_t list = RvaToAddress(GameOffsets::kBgSelectList);

	if (!IsAddressInGameModule(table) || !IsAddressInGameModule(list))
	{
		strncpy_s(g_status, "the stage table is not where this build expects it", _TRUNCATE);
		LOG("BgCeiling: %s", g_status);
		return false;
	}

	const uint32_t arrays[2] = { static_cast<uint32_t>(table), static_cast<uint32_t>(list) };

	std::vector<uint8_t*> tableSites;
	std::vector<uint8_t*> listSites;
	Sites(arrays[0], tableSites);
	Sites(arrays[1], listSites);

	std::vector<Bound> bounds;
	Bounds(arrays, 2, bounds);

	std::vector<std::vector<Cell> > cells;
	Cells(listSites, cells);

	const int tableCount = static_cast<int>(tableSites.size());
	const int listCount = static_cast<int>(listSites.size());
	const int boundCount = static_cast<int>(bounds.size());
	const int cellCount = static_cast<int>(cells.size());

	LOG("BgCeiling: %d table reference(s), %d list reference(s), %d bound check(s), %d picker "
		"list(s)", tableCount, listCount, boundCount, cellCount);

	if (tableCount < kLeastTableSites || tableCount > kMostTableSites ||
		listCount < kLeastListSites || listCount > kMostListSites ||
		boundCount < kLeastBoundSites || boundCount > kMostBoundSites)
	{
		sprintf_s(g_status, "this build reads the stage table in ways the mod does not recognise "
			"(%d/%d/%d), so the hundred stays", tableCount, listCount, boundCount);
		LOG("BgCeiling: %s", g_status);
		return false;
	}

	bool cellsOk = cellCount == kCellFunctions;

	for (const std::vector<Cell>& one : cells)
	{
		const int found = static_cast<int>(one.size());
		cellsOk = cellsOk && found >= kLeastCellSites && found <= kMostCellSites;
	}

	uint8_t* const empty = static_cast<uint8_t*>(VirtualAlloc(nullptr, kRecordBytes,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

	if (empty == nullptr)
	{
		strncpy_s(g_status, "the empty stage record could not be allocated", _TRUNCATE);
		LOG("BgCeiling: %s", g_status);
		return false;
	}

	uint8_t* const wideTable = Widened(table, kWideNumbers, kPaddedNumbers,
		static_cast<uint32_t>(reinterpret_cast<uintptr_t>(empty)));

	uint8_t* const wideList = Widened(list, kWideListEntries, kPaddedListEntries, 0);
	uint8_t* const trampolines = static_cast<uint8_t*>(VirtualAlloc(nullptr,
		bounds.size() * kTrampolineBytes, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

	if (wideTable == nullptr || wideList == nullptr || trampolines == nullptr)
	{
		strncpy_s(g_status, "the wider stage table could not be allocated", _TRUNCATE);
		LOG("BgCeiling: %s", g_status);
		return false;
	}

	int lifted = 0;

	for (size_t i = 0; i < bounds.size(); ++i)
		lifted += Emit(bounds[i], trampolines + i * kTrampolineBytes, kWideNumbers) ? 1 : 0;

	const int repointed = Repoint(tableSites, wideTable) + Repoint(listSites, wideList);

	if (lifted != boundCount || repointed != tableCount + listCount)
	{
		sprintf_s(g_status, "only %d of %d bound check(s) and %d of %d reference(s) took the "
			"write - the game is in a mixed state", lifted, boundCount, repointed,
			tableCount + listCount);
		LOG("BgCeiling: %s", g_status);
		return false;
	}

	g_lifted = true;
	g_restacked = cellsOk;
	g_table = reinterpret_cast<uintptr_t>(wideTable);

	for (const std::vector<Cell>& one : cells)
		g_restacked = g_restacked && Restack(one);

	sprintf_s(g_status, "%d stage numbers, %d picker entries (was %d)", kWideNumbers,
		ListEntries(), kStockNumbers);

	LOG("BgCeiling: %s - %d reference(s) repointed, %d bound check(s) lifted, %d picker list(s) "
		"moved off the stack", g_status, repointed, lifted, g_restacked ? cellCount : 0);

	return true;
}

bool BgCeiling::Lifted()
{
	return g_lifted;
}

int BgCeiling::Numbers()
{
	return g_lifted ? kWideNumbers : kStockNumbers;
}

bool BgCeiling::Restacked()
{
	return g_restacked;
}

uintptr_t BgCeiling::RecordTable()
{
	return g_table != 0 ? g_table : RvaToAddress(GameOffsets::kBgRecordTable);
}

int BgCeiling::ListEntries()
{
	return g_lifted && g_restacked ? kWideListEntries : kStockNumbers;
}

const char* BgCeiling::StatusText()
{
	return g_status;
}
