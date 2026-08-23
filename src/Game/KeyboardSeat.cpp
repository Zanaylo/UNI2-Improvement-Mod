#include "Game/KeyboardSeat.h"

#include "Core/Settings.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/OnlineState.h"
#include "Hooks/HookManager.h"
#include "Training/PlayerControl.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kKeyboardPort = 0;
constexpr uint32_t kPadPort = 1;

char g_status[192] = "the keyboard is where the game puts it";

int g_seat = KeyboardSeat::Seat_Default;
bool g_route = true;

volatile LONG g_releaseRequest = 0;

bool g_flagHeld = false;
uint32_t g_savedPortIsKeyboard = 0;

bool g_slotsHeld = false;
uint32_t g_savedSlot[2] = {};
uint32_t g_wroteSlot[2] = {};

typedef void(__fastcall* PadUpdateFn)(void*);
PadUpdateFn oPadUpdate = nullptr;

void* Address(uintptr_t rva)
{
	const uintptr_t at = RvaToAddress(rva);
	return at != 0 ? reinterpret_cast<void*>(at) : nullptr;
}

uint32_t ReadDword(uintptr_t rva, uint32_t fallback)
{
	uint32_t value = fallback;
	void* const at = Address(rva);

	if (at != nullptr)
		TryReadDword(at, value);

	return value;
}

bool WriteDword(uintptr_t rva, uint32_t value)
{
	void* const at = Address(rva);
	return at != nullptr && TryWriteDword(at, value);
}

uintptr_t PadPortIsKeyboardRva()
{
	return GameOffsets::kPadPortIsKeyboard + kPadPort * 4;
}

uintptr_t SlotRva(int side)
{
	return GameOffsets::kInputPadSlots + static_cast<uintptr_t>(side) * 4;
}

int KeyboardSide()
{
	return g_seat == KeyboardSeat::Seat_P2 ? 1 : 0;
}

bool Seated()
{
	return g_seat != KeyboardSeat::Seat_Default;
}

void HoldFlag()
{
	if (!g_flagHeld)
	{
		g_savedPortIsKeyboard = ReadDword(PadPortIsKeyboardRva(), 0);
		g_flagHeld = true;
	}

	if (ReadDword(PadPortIsKeyboardRva(), 0) == 0)
		WriteDword(PadPortIsKeyboardRva(), 1);
}

void HoldSlots()
{
	const uint32_t live[2] = { ReadDword(SlotRva(0), GameOffsets::kInputPadSlotNone),
		ReadDword(SlotRva(1), GameOffsets::kInputPadSlotNone) };

	if (!g_slotsHeld || live[0] != g_wroteSlot[0] || live[1] != g_wroteSlot[1])
	{
		g_savedSlot[0] = live[0];
		g_savedSlot[1] = live[1];
		g_slotsHeld = true;
	}

	const int side = KeyboardSide();

	g_wroteSlot[side] = kKeyboardPort;
	g_wroteSlot[side == 0 ? 1 : 0] = kPadPort;

	WriteDword(SlotRva(0), g_wroteSlot[0]);
	WriteDword(SlotRva(1), g_wroteSlot[1]);
}

void ReleaseNow()
{
	if (g_slotsHeld)
	{
		WriteDword(SlotRva(0), g_savedSlot[0]);
		WriteDword(SlotRva(1), g_savedSlot[1]);
		g_slotsHeld = false;
	}

	if (g_flagHeld)
	{
		WriteDword(PadPortIsKeyboardRva(), g_savedPortIsKeyboard);
		g_flagHeld = false;
	}
}

void ExchangePortRecords()
{
	uint8_t* const first = static_cast<uint8_t*>(Address(GameOffsets::kPadPortState));
	if (first == nullptr)
		return;

	uint8_t* const second = first + GameOffsets::kPadPortStateStride;

	uint8_t a[GameOffsets::kPadPortStateStride] = {};
	uint8_t b[GameOffsets::kPadPortStateStride] = {};

	if (!TryReadMemory(a, first, sizeof(a)) || !TryReadMemory(b, second, sizeof(b)))
		return;

	TryWriteMemory(first, b, sizeof(b));
	TryWriteMemory(second, a, sizeof(a));
}

void Describe()
{
	if (!Seated())
	{
		sprintf_s(g_status, "the keyboard is where the game puts it");
		return;
	}

	sprintf_s(g_status, "the keyboard plays %s, a controller plays %s",
		KeyboardSide() == 0 ? "1P" : "2P", KeyboardSide() == 0 ? "2P" : "1P");
}

void __fastcall HookedPadUpdate(void* self)
{
	const bool exchange = Seated() && !OnlineState::IsOnline();

	if (exchange)
		ExchangePortRecords();

	oPadUpdate(self);

	if (exchange)
		ExchangePortRecords();
}

}

bool KeyboardSeat::Initialize()
{
	void* const target = Address(GameOffsets::kFnPadUpdate);

	if (target == nullptr || !HookManager::CreateAndEnableHook(target, &HookedPadUpdate,
		reinterpret_cast<void**>(&oPadUpdate), "PadUpdate"))
	{
		LOG("keyboard seat: could not hook the pad update");
		return false;
	}

	return true;
}

void KeyboardSeat::ApplySaved()
{
	g_seat = g_modVals.keyboardSeat;
	g_route = g_modVals.keyboardSeatRouteSides;

	Describe();
}

void KeyboardSeat::SetSeat(int seat)
{
	if (seat < Seat_Default || seat > Seat_P2 || seat == g_seat)
		return;

	g_seat = seat;

	if (!Seated())
		InterlockedExchange(&g_releaseRequest, 1);

	Describe();
	LOG("keyboard seat: %s", g_status);

	g_modVals.keyboardSeat = g_seat;
	Settings::SaveInt("Input", "KeyboardSeat", g_seat);
}

int KeyboardSeat::GetSeat()
{
	return g_seat;
}

void KeyboardSeat::SetRouteSides(bool route)
{
	if (route == g_route)
		return;

	g_route = route;

	if (!route)
		InterlockedExchange(&g_releaseRequest, 1);

	g_modVals.keyboardSeatRouteSides = route;
	Settings::SaveInt("Input", "KeyboardSeatRouteSides", route ? 1 : 0);
}

bool KeyboardSeat::GetRouteSides()
{
	return g_route;
}

void KeyboardSeat::OnFrameUpdate()
{
	if (InterlockedExchange(&g_releaseRequest, 0) != 0)
		ReleaseNow();

	if (!Seated() || OnlineState::IsOnline())
	{
		ReleaseNow();
		return;
	}

	HoldFlag();

	if (g_route && !PlayerControl::IsDriving())
		HoldSlots();
}

void KeyboardSeat::Update()
{
	if (GameState::IsInMatch())
		return;

	if (InterlockedExchange(&g_releaseRequest, 0) != 0)
		ReleaseNow();

	if (!Seated() || OnlineState::IsOnline())
	{
		ReleaseNow();
		return;
	}

	HoldFlag();

	if (g_route)
		HoldSlots();
}

bool KeyboardSeat::IsAvailable()
{
	return oPadUpdate != nullptr;
}

const char* KeyboardSeat::GetStatus()
{
	return g_status;
}

const char* KeyboardSeat::GetSeatName(int seat)
{
	switch (seat)
	{
	case Seat_P1:
		return "1P";
	case Seat_P2:
		return "2P";
	default:
		return "Game default";
	}
}
