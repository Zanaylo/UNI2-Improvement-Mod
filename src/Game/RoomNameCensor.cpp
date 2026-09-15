#include "Game/RoomNameCensor.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/NameCensor.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

typedef void(__thiscall* SetRoomRowsFn)(void*, uint8_t*, int);

constexpr int kMostRows = 0x20;
constexpr uint32_t kShortString = 0x10;

SetRoomRowsFn oSetRoomRows = nullptr;

bool g_enabled = false;
volatile long g_covered = 0;
char g_status[160] = {};

void Summarise()
{
	if (oSetRoomRows == nullptr)
	{
		strncpy_s(g_status, "the game's room list code is not where this game version expects it", _TRUNCATE);
		return;
	}

	if (!g_enabled)
	{
		strncpy_s(g_status, "off, room names are shown as they are", _TRUNCATE);
		return;
	}

	sprintf_s(g_status, "on, every room in the search is shown as '%s'", NameCensor::Mask());
}

void Cover(uint8_t* row)
{
	uint8_t* const name = row + GameOffsets::kRoomRowName;
	uint32_t* const size = reinterpret_cast<uint32_t*>(name + GameOffsets::kRoomRowNameSize);
	const uint32_t capacity = *reinterpret_cast<uint32_t*>(name + GameOffsets::kRoomRowNameCapacity);

	if (*size == 0)
		return;

	const char* const mask = NameCensor::Mask();
	const size_t length = strlen(mask);
	const size_t written = length < capacity ? length : capacity;
	char* const text = capacity >= kShortString ? *reinterpret_cast<char**>(name) : reinterpret_cast<char*>(name);

	memcpy(text, mask, written);
	text[written] = 0;
	*size = static_cast<uint32_t>(written);

	InterlockedIncrement(&g_covered);
}

void CoverAll(uint8_t* rows, int count)
{
	if (!g_enabled || rows == nullptr)
		return;

	const int covered = count < kMostRows ? count : kMostRows;

	__try
	{
		for (int i = 0; i < covered; ++i)
			Cover(rows + i * GameOffsets::kRoomRowStride);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		strncpy_s(g_status, "a room row could not be read, so names in it are shown as they are", _TRUNCATE);
	}
}

void __fastcall HookedSetRoomRows(void* list, void*, uint8_t* rows, int count)
{
	CoverAll(rows, count);
	oSetRoomRows(list, rows, count);
}

}

bool RoomNameCensor::Install()
{
	if (oSetRoomRows != nullptr)
		return true;

	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnSetRoomRows));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)) ||
		!HookManager::CreateAndEnableHook(target, &HookedSetRoomRows, reinterpret_cast<void**>(&oSetRoomRows),
			"SetRoomRows"))
	{
		oSetRoomRows = nullptr;
		Summarise();
		LOG("RoomNameCensor: %s", g_status);
		return false;
	}

	Summarise();
	LOG("RoomNameCensor: %s", g_status);
	return true;
}

bool RoomNameCensor::IsAvailable()
{
	return oSetRoomRows != nullptr;
}

bool RoomNameCensor::IsEnabled()
{
	return g_enabled;
}

void RoomNameCensor::SetEnabled(bool enabled)
{
	g_enabled = enabled;
	Summarise();
}

int RoomNameCensor::Count()
{
	return static_cast<int>(g_covered);
}

const char* RoomNameCensor::StatusText()
{
	Summarise();
	return g_status;
}
