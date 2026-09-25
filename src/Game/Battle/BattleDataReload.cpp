#include "Game/Battle/BattleDataReload.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using LoaderFn = int(__cdecl*)();

constexpr size_t kPrologueBytes = 10;
constexpr uintptr_t kPreferredBase = 0x400000;

struct Loader
{
	uintptr_t rva;
	const char* name;
	uint8_t prologue[kPrologueBytes];
	size_t length;
	size_t addressAt;
	bool reportsSuccess;
};

constexpr Loader kLoaders[] = {
	{ GameOffsets::kFnReloadBattleScripts, "the battle scripts",
		{ 0x51, 0xb9, 0x14, 0xd6, 0xf2, 0x03, 0xe8 }, 7, 2, true },
	{ GameOffsets::kFnLoadVectorTable, "VectorTable",
		{ 0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x4e, 0xd7, 0x8f, 0x00 }, 10, 6, false },
	{ GameOffsets::kFnLoadBattleInfo, "BattleInfo",
		{ 0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x18, 0xdb, 0x8f, 0x00 }, 10, 6, false },
};

int g_supported = -1;
int g_ran = 0;
char g_status[192] = "not tried yet";

bool ReadDword(uintptr_t address, uint32_t& out)
{
	return TryReadDword(reinterpret_cast<const void*>(address), out);
}

bool Matches(const Loader& loader)
{
	const uintptr_t address = RvaToAddress(loader.rva);

	if (!IsAddressInGameModule(address))
		return false;

	uint8_t bytes[kPrologueBytes] = {};

	if (!TryReadMemory(bytes, reinterpret_cast<const void*>(address), loader.length))
		return false;

	uint8_t expected[kPrologueBytes] = {};
	memcpy(expected, loader.prologue, loader.length);

	uint32_t absolute = 0;
	memcpy(&absolute, expected + loader.addressAt, sizeof(absolute));
	absolute += static_cast<uint32_t>(RvaToAddress(0) - kPreferredBase);
	memcpy(expected + loader.addressAt, &absolute, sizeof(absolute));

	return memcmp(bytes, expected, loader.length) == 0;
}

bool Call(const Loader& loader, int& outResult)
{
	const LoaderFn loaderFn = reinterpret_cast<LoaderFn>(RvaToAddress(loader.rva));

	__try
	{
		outResult = loaderFn();
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

bool NoLoadingThread()
{
	uint32_t running = 1;

	return ReadDword(RvaToAddress(GameOffsets::kRunningResourceThread), running) && running == 0;
}

bool VmBuilt()
{
	uint32_t created = 0;

	return ReadDword(RvaToAddress(GameOffsets::kBattleVmCreated), created) && (created & 0xff) != 0;
}

}

bool BattleDataReload::IsSupported()
{
	if (g_supported >= 0)
		return g_supported != 0;

	g_supported = 1;

	for (const Loader& loader : kLoaders)
	{
		if (Matches(loader))
			continue;

		g_supported = 0;
		sprintf_s(g_status, "%s is not where this game version expects it", loader.name);
		LOG("BattleDataReload: %s", g_status);
	}

	return g_supported != 0;
}

bool BattleDataReload::CanRunNow()
{
	return IsSupported() && !GameState::IsInMatch() && NoLoadingThread() && VmBuilt();
}

bool BattleDataReload::Run()
{
	if (!CanRunNow())
	{
		strncpy_s(g_status, "not now: a match or a load is running, or the battle data is not "
			"ready yet", _TRUNCATE);
		return false;
	}

	const int before = VectorRecords();
	const DWORD started = GetTickCount();

	for (const Loader& loader : kLoaders)
	{
		int result = 0;

		if (Call(loader, result) && (!loader.reportsSuccess || result != 0))
			continue;

		sprintf_s(g_status, "%s failed, so the battle data may be half rebuilt", loader.name);
		LOG("BattleDataReload: %s", g_status);
		return false;
	}

	const int after = VectorRecords();

	++g_ran;

	sprintf_s(g_status, "rebuilt in %u ms, VectorTable %d -> %d record(s), %d time(s) this session",
		static_cast<unsigned>(GetTickCount() - started), before, after, g_ran);

	LOG("BattleDataReload: %s", g_status);
	return after > 0;
}

int BattleDataReload::VectorRecords()
{
	uint32_t container = 0;
	uint32_t set = 0;
	uint32_t begin = 0;
	uint32_t end = 0;

	if (!ReadDword(RvaToAddress(GameOffsets::kVectorTable), container) || container == 0)
		return -1;

	if (!ReadDword(container + GameOffsets::kVectorSetAt, set) || set == 0)
		return -1;

	if (!ReadDword(set + GameOffsets::kVectorRecordsBeginAt, begin) ||
		!ReadDword(set + GameOffsets::kVectorRecordsEndAt, end) || end < begin)
	{
		return -1;
	}

	return static_cast<int>((end - begin) / GameOffsets::kVectorRecordStride);
}

const char* BattleDataReload::StatusText()
{
	return g_status;
}
