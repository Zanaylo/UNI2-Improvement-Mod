#include "Game/ScriptReload.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

typedef int(__thiscall* LoadScriptFn)(void* context, const char* path, const char* root);

struct Script
{
	uintptr_t path;
	uintptr_t root;
	const char* expectedPath;
	const char* expectedRoot;
};

constexpr Script kScripts[] = {
	{
		GameOffsets::kBattleInitPath, GameOffsets::kBattleInitRoot,
		"./script/Battle_Init.txt", "BattleInit"
	},
	{
		GameOffsets::kBattleStdPath, GameOffsets::kBattleStdRoot,
		"./script/Battle_Std.txt", "BattleScript"
	},
	{
		GameOffsets::kComBasePath, GameOffsets::kComBaseRoot,
		"./data/_combase.txt", "_com_base_"
	}
};

constexpr int kScriptCount = static_cast<int>(sizeof(kScripts) / sizeof(kScripts[0]));

int g_ran = 0;
char g_status[192] = "not tried yet";

const char* Text(uintptr_t rva)
{
	const uintptr_t address = RvaToAddress(rva);

	if (!IsAddressInGameModule(address) || !IsReadableMemory(reinterpret_cast<void*>(address), 1))
		return nullptr;

	return reinterpret_cast<const char*>(address);
}

bool Named(const Script& script)
{
	const char* const path = Text(script.path);
	const char* const root = Text(script.root);

	if (path == nullptr || root == nullptr)
		return false;

	return strcmp(path, script.expectedPath) == 0 && strcmp(root, script.expectedRoot) == 0;
}

void* Context()
{
	const uintptr_t slot = RvaToAddress(GameOffsets::kBattleScriptContext);

	if (!IsAddressInGameModule(slot))
		return nullptr;

	uint32_t value = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(slot), value) || value == 0)
		return nullptr;

	void* const context = reinterpret_cast<void*>(static_cast<uintptr_t>(value));

	return IsReadableMemory(context, 8) ? context : nullptr;
}

LoadScriptFn Entry()
{
	const uintptr_t address = RvaToAddress(GameOffsets::kFnLoadBattleScript);

	if (!IsAddressInGameModule(address))
		return nullptr;

	return reinterpret_cast<LoadScriptFn>(address);
}

}

bool ScriptReload::IsSupported()
{
	if (Entry() == nullptr)
		return false;

	for (const Script& script : kScripts)
	{
		if (!Named(script))
			return false;
	}

	return true;
}

bool ScriptReload::Run()
{
	if (!IsSupported())
	{
		strncpy_s(g_status, "this game build does not name the battle scripts where expected",
			_TRUNCATE);
		return false;
	}

	if (GameState::IsInMatch())
	{
		strncpy_s(g_status, "not while a match is running", _TRUNCATE);
		return false;
	}

	void* const context = Context();

	if (context == nullptr)
	{
		strncpy_s(g_status, "the game has not built its battle script context yet", _TRUNCATE);
		return false;
	}

	const LoadScriptFn load = Entry();
	const DWORD started = GetTickCount();

	for (const Script& script : kScripts)
	{
		const char* const path = Text(script.path);

		LOG("ScriptReload: running %s into %s", script.expectedPath, script.expectedRoot);

		if (load(context, path, Text(script.root)) != 0)
		{
			LOG("ScriptReload: %s came back ok", script.expectedPath);
			continue;
		}

		sprintf_s(g_status, "%s would not compile - the tables are half rebuilt, restart the game",
			script.expectedPath);

		LOG("ScriptReload: %s", g_status);
		return false;
	}

	++g_ran;

	sprintf_s(g_status, "battle tables rebuilt in %ums, %d time(s) this session",
		static_cast<unsigned>(GetTickCount() - started), g_ran);

	LOG("ScriptReload: %s", g_status);
	return true;
}

int ScriptReload::Count()
{
	return g_ran;
}

const char* ScriptReload::StatusText()
{
	return g_status;
}
