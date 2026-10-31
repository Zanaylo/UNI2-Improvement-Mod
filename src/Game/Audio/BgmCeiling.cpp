#include "Game/Audio/BgmCeiling.h"

#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Audio/BgmTableSites.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr const char* kSection = "Music";
constexpr const char* kKey = "WidenMusicTable";

using Rewrite = uint32_t (*)(uint32_t value, const BgmTableSites::Table& from, const BgmTableSites::Table& to);

bool g_lifted = false;
uintptr_t g_table = 0;
char g_status[192] = "the game's 200 music slots";

bool Wanted()
{
	char stored[16] = {};

	Ini::GetString(kSection, kKey, "1", stored, sizeof(stored), Settings::GetIniPath().c_str());

	return stored[0] != '0';
}

bool Fail(const char* why)
{
	strncpy_s(g_status, why, _TRUNCATE);
	LOG("BgmCeiling: %s", g_status);
	return false;
}

uint32_t Read(uintptr_t text, size_t at)
{
	uint32_t value = 0;
	memcpy(&value, reinterpret_cast<const void*>(text + at), sizeof(value));

	return value;
}

bool Put(uintptr_t text, size_t at, uint32_t value)
{
	return WriteCodeBytes(reinterpret_cast<void*>(text + at), &value, sizeof(value));
}

struct Patch
{
	size_t at;
	uint32_t was;
	uint32_t now;
};

void Plan(uintptr_t text, const std::vector<size_t>& sites, Rewrite rewrite, const BgmTableSites::Table& from,
	const BgmTableSites::Table& to, std::vector<Patch>& out)
{
	for (size_t at : sites)
	{
		const uint32_t was = Read(text, at);
		out.push_back({ at, was, rewrite(was, from, to) });
	}
}

bool Apply(uintptr_t text, const std::vector<Patch>& patches)
{
	for (size_t i = 0; i < patches.size(); ++i)
	{
		if (Put(text, patches[i].at, patches[i].now))
			continue;

		for (size_t undo = 0; undo < i; ++undo)
			Put(text, patches[undo].at, patches[undo].was);

		return false;
	}

	return true;
}

uint32_t Cleared(uint32_t, const BgmTableSites::Table&, const BgmTableSites::Table& to)
{
	return BgmTableSites::Clear(to);
}

int AlreadyRead(uintptr_t stock)
{
	int present = 0;

	for (int id = 0; id < BgmCeiling::kStockSlots; ++id)
	{
		uint32_t field = 0;
		const uintptr_t record = stock + id * GameOffsets::kBgmTableStride + GameOffsets::kBgmPresent;

		if (TryReadDword(reinterpret_cast<const void*>(record), field) && field != 0)
			++present;
	}

	return present;
}

uint8_t* Allocate(uintptr_t stock)
{
	const size_t bytes = static_cast<size_t>(BgmCeiling::kWideSlots) * GameOffsets::kBgmTableStride;
	uint8_t* const wide = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE));

	if (wide == nullptr)
		return nullptr;

	memcpy(wide, reinterpret_cast<const void*>(stock),
		static_cast<size_t>(BgmCeiling::kStockSlots) * GameOffsets::kBgmTableStride);

	return wide;
}

}

bool BgmCeiling::Initialize()
{
	if (!Wanted())
		return Fail("off, the game keeps its 200 music slots");

	uintptr_t text = 0;
	size_t textBytes = 0;

	if (!HookManager::GetSectionBounds(".text", text, textBytes))
		return Fail("the game's code could not be read");

	const uintptr_t stock = RvaToAddress(GameOffsets::kBgmTableBase);

	if (!IsAddressInGameModule(stock))
		return Fail("the music table is not where this game version expects it");

	const BgmTableSites::Table from = { static_cast<uint32_t>(stock),
		static_cast<uint32_t>(GameOffsets::kBgmTableStride), kStockSlots };
	const BgmTableSites::Plan plan = BgmTableSites::Find(reinterpret_cast<const uint8_t*>(text), textBytes, from);

	LOG("BgmCeiling: %d field reference(s), %d table end(s), %d bound check(s), %d clear(s)",
		static_cast<int>(plan.fields.size()), static_cast<int>(plan.ends.size()),
		static_cast<int>(plan.bounds.size()), static_cast<int>(plan.clears.size()));

	if (!BgmTableSites::Expected(plan))
		return Fail("this game version reads the music table in a way the mod does not know, so it keeps 200 slots");

	uint8_t* const wide = Allocate(stock);

	if (wide == nullptr)
		return Fail("the wider music table could not be allocated");

	const int read = AlreadyRead(stock);
	const BgmTableSites::Table to = { static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wide)), from.stride, kWideSlots };

	std::vector<Patch> patches;
	Plan(text, plan.fields, &BgmTableSites::Field, from, to, patches);
	Plan(text, plan.ends, &BgmTableSites::End, from, to, patches);
	Plan(text, plan.bounds, &BgmTableSites::Bound, from, to, patches);
	Plan(text, plan.clears, &Cleared, from, to, patches);

	if (!Apply(text, patches))
	{
		VirtualFree(wide, 0, MEM_RELEASE);
		return Fail("the game's code could not be written, so it keeps 200 music slots");
	}

	g_table = reinterpret_cast<uintptr_t>(wide);
	g_lifted = true;

	sprintf_s(g_status, "%d music slots, packs use 100 to %d", kWideSlots, kWideSlots - 2);
	LOG("BgmCeiling: %s%s", g_status, read == 0 ? ""
		: " - the game had already read its music table, so only its first 200 slots are filled this session");

	return true;
}

bool BgmCeiling::Lifted()
{
	return g_lifted;
}

int BgmCeiling::Slots()
{
	return g_lifted ? kWideSlots : kStockSlots;
}

uintptr_t BgmCeiling::Table()
{
	return g_lifted ? g_table : RvaToAddress(GameOffsets::kBgmTableBase);
}

const char* BgmCeiling::StatusText()
{
	return g_status;
}
