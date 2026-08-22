#include "Training/PlayerControl.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Hooks/HookManager.h"

#include <MinHook.h>
#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

char g_status[128] = "not driving anything";

PlayerControl::Mode g_mode = PlayerControl::Mode_Mine;

uint8_t g_heldLever = 0;
uint8_t g_heldButtons = 0;

uint8_t g_tapLever = 0;
uint8_t g_tapButtons = 0;
int g_tapFrames = 0;

constexpr int kTapFrames = 4;

PlayerControl::Input g_script[2][PlayerControl::kMaxScript] = {};
int g_scriptCount[2] = {};
int g_scriptFrame[2] = {};

bool g_alive = false;

bool g_routed = false;
bool g_primed = false;
uint32_t g_savedPad[2] = {};
uint32_t g_savedRecorderMode = 0;
uint32_t g_savedRecorderRunning = 0;
uint32_t g_savedEnemyStatus = 0;

typedef void(__fastcall* FetchPadFn)(void*, int);
FetchPadFn oFetchPad = nullptr;

uint32_t GetRecorderField(uintptr_t offset)
{
	uint32_t value = 0;
	const uintptr_t recorder = RvaToAddress(GameOffsets::kRecorderObject);

	if (recorder != 0)
		TryReadDword(reinterpret_cast<const void*>(recorder + offset), value);

	return value;
}

uint32_t GetEnemyStatus()
{
	uint32_t value = 0;
	const uintptr_t at = RvaToAddress(GameOffsets::kEnemyStatus);

	if (at == 0 || !TryReadDword(reinterpret_cast<const void*>(at), value))
		return 0;

	return value;
}

bool SetEnemyStatus(uint32_t value)
{
	const uintptr_t at = RvaToAddress(GameOffsets::kEnemyStatus);
	return at != 0 && TryWriteDword(reinterpret_cast<void*>(at), value);
}

uint32_t GetPadSlot(int player)
{
	uint32_t value = GameOffsets::kInputPadSlotNone;
	const uintptr_t base = RvaToAddress(GameOffsets::kInputPadSlots);

	if (base != 0)
		TryReadDword(reinterpret_cast<const void*>(base + static_cast<uintptr_t>(player) * 4), value);

	return value;
}

bool SetPadSlot(int player, uint32_t slot)
{
	const uintptr_t base = RvaToAddress(GameOffsets::kInputPadSlots);
	if (base == 0)
		return false;

	return TryWriteDword(reinterpret_cast<void*>(base + static_cast<uintptr_t>(player) * 4), slot);
}

int g_buttonByte[4] = { 3, 2, 0, 1 };

void WriteStruct(void* out, uint8_t lever, uint8_t buttons)
{
	uint8_t* const bytes = static_cast<uint8_t*>(out);

	bytes[0x04] = lever;

	for (int i = 0; i < 12; ++i)
		bytes[0x08 + i] = 0;

	for (int bit = 0; bit < 4; ++bit)
	{
		if ((buttons & (1u << bit)) != 0)
			bytes[0x08 + g_buttonByte[bit]] = 1;
	}
}

constexpr int kCalibrateByteCount = 12;
constexpr int kCalibrateHoldFrames = 6;
constexpr int kCalibrateSampleFrame = 4;
constexpr int kCalibrateGapFrames = 3;

bool g_calibrating = false;
volatile LONG g_releaseRequest = 0;
volatile LONG g_calibrateRequest = 0;
int g_calibrateByte = 0;
int g_calibratePhase = 0;
int g_calibrateFrame = 0;
int g_calibrateFoundByte[4] = { -1, -1, -1, -1 };

bool PrimePlayback()
{
	const uintptr_t container = RvaToAddress(GameOffsets::kRecorderContainer);
	const uintptr_t recorder = RvaToAddress(GameOffsets::kRecorderObject);

	if (container == 0 || recorder == 0)
		return false;

	const int slot = GameOffsets::kRecorderSlotCount - 1;
	const uint32_t start = static_cast<uint32_t>(slot) * GameOffsets::kRecorderSlotSpan;

	const uintptr_t record = container + GameOffsets::kRecorderTakeTable +
		static_cast<uintptr_t>(slot) * GameOffsets::kRecorderTakeStride;

	TryWriteUnaligned(reinterpret_cast<void*>(container + GameOffsets::kRecorderDataBase + start), 0);
	TryWriteDword(reinterpret_cast<void*>(record + GameOffsets::kRecorderTakeStart), start);
	TryWriteDword(reinterpret_cast<void*>(record + GameOffsets::kRecorderTakeLength), 4);

	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderMode),
		GameOffsets::kRecorderModePlayback);
	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderSlot),
		static_cast<uint32_t>(slot));
	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderCursor), 0);
	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderStarted), 1);
	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderTake),
		static_cast<uint32_t>(record));
	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderLength), 1);
	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderCapacity), 1);
	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderLiveInput), 0);
	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderOverran), 0);
	TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderRunning), 1);

	LOG("player control: primed playback on slot %d", slot + 1);
	return true;
}

void SaveOnce()
{
	if (g_routed)
		return;

	g_savedPad[0] = GetPadSlot(0);
	g_savedPad[1] = GetPadSlot(1);
	g_savedEnemyStatus = GetEnemyStatus();
	g_savedRecorderMode = GetRecorderField(GameOffsets::kRecorderMode);
	g_savedRecorderRunning = GetRecorderField(GameOffsets::kRecorderRunning);
	g_routed = true;

	LOG("player control: pads were %d and %d, enemy status %u",
		static_cast<int>(g_savedPad[0]), static_cast<int>(g_savedPad[1]), g_savedEnemyStatus);
}

void PutBack()
{
	if (!g_routed)
		return;

	SetPadSlot(0, g_savedPad[0]);
	SetPadSlot(1, g_savedPad[1]);
	SetEnemyStatus(g_savedEnemyStatus);

	const uintptr_t recorder = RvaToAddress(GameOffsets::kRecorderObject);
	if (recorder != 0 && g_primed)
	{
		TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderRunning),
			g_savedRecorderRunning);
		TryWriteDword(reinterpret_cast<void*>(recorder + GameOffsets::kRecorderMode),
			g_savedRecorderMode);
	}

	g_primed = false;
	g_routed = false;
}

void ClearInputHold()
{
	for (int i = 0; i < 2; ++i)
	{
		uint8_t* const chara = static_cast<uint8_t*>(MemoryMap::GetCharaSlot(i));
		if (chara == nullptr)
			continue;

		uint8_t* const at = chara + GameOffsets::kPlayerDataInputHold;

		uint32_t word = 0;
		if (TryReadUnaligned(at, word))
			TryWriteUnaligned(at, word & 0xffffff00u);
	}
}

bool AnythingDriving()
{
	return g_mode != PlayerControl::Mode_Mine || g_heldLever != 0 || g_heldButtons != 0 ||
		g_tapFrames > 0 || g_scriptFrame[0] < g_scriptCount[0] ||
		g_scriptFrame[1] < g_scriptCount[1];
}

void SampleCalibration()
{
	PlayerControl::Input input = {};
	if (!PlayerControl::ReadInput(PlayerControl::GetHomeSide(), input) || input.buttons == 0)
		return;

	int found = -1;

	for (int bit = 0; bit < 4; ++bit)
	{
		if ((input.buttons & (1u << bit)) == 0)
			continue;

		if (found >= 0)
			return;

		found = bit;
	}

	if (found >= 0 && g_calibrateFoundByte[found] < 0)
		g_calibrateFoundByte[found] = g_calibrateByte;
}

void FinishCalibration()
{
	g_calibrating = false;

	bool ok = true;
	for (int bit = 0; bit < 4; ++bit)
		ok = ok && g_calibrateFoundByte[bit] >= 0;

	if (!ok)
	{
		sprintf_s(g_status, "calibration could not find all four buttons - kept the previous mapping");
		LOG("player control: %s", g_status);
		return;
	}

	for (int bit = 0; bit < 4; ++bit)
		g_buttonByte[bit] = g_calibrateFoundByte[bit];

	sprintf_s(g_status, "calibrated: A=byte%d B=byte%d C=byte%d D=byte%d", g_buttonByte[0],
		g_buttonByte[1], g_buttonByte[2], g_buttonByte[3]);
	LOG("player control: %s", g_status);
}

void UpdateCalibration()
{
	ClearInputHold();

	++g_calibrateFrame;

	if (g_calibratePhase == 0)
	{
		if (g_calibrateFrame == kCalibrateSampleFrame)
			SampleCalibration();

		if (g_calibrateFrame < kCalibrateHoldFrames)
		{
			sprintf_s(g_status, "calibrating button %d of %d", g_calibrateByte + 1,
				kCalibrateByteCount);
			return;
		}

		g_calibratePhase = 1;
		g_calibrateFrame = 0;
		return;
	}

	if (g_calibrateFrame < kCalibrateGapFrames)
		return;

	g_calibratePhase = 0;
	g_calibrateFrame = 0;
	++g_calibrateByte;

	if (g_calibrateByte >= kCalibrateByteCount)
		FinishCalibration();
}

void ReleaseNow()
{
	if (!g_routed && !g_calibrating && !AnythingDriving())
		return;

	g_mode = PlayerControl::Mode_Mine;

	g_heldLever = 0;
	g_heldButtons = 0;
	g_tapFrames = 0;

	g_calibrating = false;

	PlayerControl::StopScript(0);
	PlayerControl::StopScript(1);

	PutBack();

	sprintf_s(g_status, "not driving anything");
}

void StartCalibration()
{
	ReleaseNow();

	g_calibrating = true;
	g_calibrateByte = 0;
	g_calibratePhase = 0;
	g_calibrateFrame = 0;

	for (int bit = 0; bit < 4; ++bit)
		g_calibrateFoundByte[bit] = -1;

	sprintf_s(g_status, "calibrating button 1 of %d", kCalibrateByteCount);
}

void __fastcall HookedFetchPad(void* out, int player)
{
	oFetchPad(out, player);
	PlayerControl::OnPadFetched(out, player);
}

}

bool PlayerControl::Initialize()
{
	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnFetchPad));

	if (!HookManager::CreateAndEnableHook(target, &HookedFetchPad,
		reinterpret_cast<void**>(&oFetchPad), "FetchPad"))
	{
		LOG("player control: could not hook the pad fetch");
		return false;
	}

	LOG("player control: hooked the pad fetch at 0x%p", target);
	return true;
}

bool PlayerControl::ReadInput(int player, Input& out)
{
	out.lever = 0;
	out.buttons = 0;

	if (player < 0 || player > 1 || !GameState::AllowsTrainingTools())
		return false;

	const uint8_t* const chara = static_cast<const uint8_t*>(MemoryMap::GetCharaSlot(player));
	if (chara == nullptr)
		return false;

	uint32_t word = 0;
	if (!TryReadDword(chara + GameOffsets::kPlayerDataCurrentInput, word))
		return false;

	out.lever = static_cast<uint8_t>((word >> GameOffsets::kInputLeverShift) & 0xffu);
	out.buttons = static_cast<uint8_t>(word & GameOffsets::kInputButtonMask);

	return true;
}

int PlayerControl::GetHomeSide()
{
	uint32_t side = 0;
	const uintptr_t at = RvaToAddress(GameOffsets::kPlayerSideIndex);

	if (at == 0 || !TryReadDword(reinterpret_cast<const void*>(at), side))
		return 0;

	return side == 0 ? 0 : 1;
}

int PlayerControl::GetDummySide()
{
	return GetHomeSide() == 0 ? 1 : 0;
}

void PlayerControl::SetMode(Mode mode)
{
	g_mode = mode;
}

PlayerControl::Mode PlayerControl::GetMode()
{
	return g_mode;
}

void PlayerControl::SetHeld(uint8_t lever, uint8_t buttons)
{
	g_heldLever = lever;
	g_heldButtons = buttons;
}

void PlayerControl::Tap(uint8_t lever, uint8_t buttons)
{
	g_tapLever = lever;
	g_tapButtons = buttons;
	g_tapFrames = kTapFrames;
}

void PlayerControl::RunScript(int player, const Input* frames, int count)
{
	if (player < 0 || player > 1)
		return;

	StopScript(player);

	if (frames == nullptr || count <= 0)
		return;

	const int clamped = count < kMaxScript ? count : kMaxScript;

	for (int i = 0; i < clamped; ++i)
		g_script[player][i] = frames[i];

	g_scriptCount[player] = clamped;

	g_scriptFrame[player] = -1;

	sprintf_s(g_status, "P%d running %d frames", player + 1, clamped);
}

void PlayerControl::StopScript(int player)
{
	if (player < 0 || player > 1)
		return;

	g_scriptCount[player] = 0;
	g_scriptFrame[player] = 0;
}

bool PlayerControl::IsScriptRunning(int player)
{
	if (player < 0 || player > 1)
		return false;

	return g_scriptFrame[player] < g_scriptCount[player];
}

int PlayerControl::GetScriptFrame(int player)
{
	if (player < 0 || player > 1 || g_scriptFrame[player] < 0)
		return 0;

	return g_scriptFrame[player];
}

void PlayerControl::Release()
{
	InterlockedExchange(&g_releaseRequest, 1);
}

bool PlayerControl::IsDriving()
{
	return g_routed;
}

void PlayerControl::Calibrate()
{
	if (!GameState::AllowsTrainingTools())
	{
		sprintf_s(g_status, "calibration needs a match");
		return;
	}

	InterlockedExchange(&g_calibrateRequest, 1);
	sprintf_s(g_status, "calibrating button 1 of %d", kCalibrateByteCount);
}

bool PlayerControl::IsCalibrating()
{
	return g_calibrating || InterlockedCompareExchange(&g_calibrateRequest, 0, 0) != 0;
}

void PlayerControl::KeepAlive()
{
	g_alive = true;
}

void PlayerControl::Update()
{
	const bool alive = g_alive;
	g_alive = false;

	if (!GameState::AllowsTrainingTools())
	{
		Release();
		return;
	}

	if (alive || IsCalibrating() || IsScriptRunning(0) || IsScriptRunning(1))
		return;

	if (!g_routed)
		return;

	Release();
}

void PlayerControl::OnFrameUpdate()
{
	if (InterlockedExchange(&g_releaseRequest, 0) != 0)
		ReleaseNow();

	if (!GameState::AllowsTrainingTools())
	{
		InterlockedExchange(&g_calibrateRequest, 0);
		ReleaseNow();
		return;
	}

	if (InterlockedExchange(&g_calibrateRequest, 0) != 0)
		StartCalibration();

	if (g_calibrating)
	{
		UpdateCalibration();
		return;
	}

	if (!AnythingDriving())
	{
		if (g_routed)
		{
			PutBack();
			sprintf_s(g_status, "not driving anything");
		}

		return;
	}

	for (int player = 0; player < 2; ++player)
	{
		if (g_scriptFrame[player] >= g_scriptCount[player])
			continue;

		++g_scriptFrame[player];

		if (g_scriptFrame[player] >= g_scriptCount[player])
			sprintf_s(g_status, "P%d script finished", player + 1);
		else
			sprintf_s(g_status, "P%d frame %d of %d", player + 1, g_scriptFrame[player],
				g_scriptCount[player]);
	}

	if (g_tapFrames > 0)
		--g_tapFrames;

	SaveOnce();

	const int home = GetHomeSide();
	const int away = home == 0 ? 1 : 0;

	if (g_mode != Mode_Mine)
	{
		SetPadSlot(away, g_savedPad[home]);
		SetPadSlot(home, g_mode == Mode_Other
			? GameOffsets::kInputPadSlotNone : g_savedPad[home]);
	}

	SetEnemyStatus(GameOffsets::kEnemyStatusController);
	ClearInputHold();

	if (g_mode == Mode_Both &&
		GetRecorderField(GameOffsets::kRecorderMode) != GameOffsets::kRecorderModePlayback)
	{
		g_primed = PrimePlayback() || g_primed;
	}

}

namespace {

bool ReadPadRecord(int player, uint8_t* buttons)
{
	const uintptr_t base = RvaToAddress(GameOffsets::kPadRecordBase);
	if (base == 0 || player < 0 || player > 1)
		return false;

	const uintptr_t record = base + static_cast<uintptr_t>(player) * GameOffsets::kPadRecordStride;
	bool any = false;

	for (int i = 0; i < 12; ++i)
	{
		uint32_t held = 0;
		const uintptr_t at = record + GameOffsets::kPadRecordButtons + static_cast<uintptr_t>(i);

		buttons[i] = TryReadUnaligned(reinterpret_cast<const void*>(at), held) && (held & 0xffu) != 0
			? 1 : 0;

		any = any || buttons[i] != 0;
	}

	return any;
}

void DriveFetched(void* out, int player)
{
	const int home = PlayerControl::GetHomeSide();
	const int dummy = home == 0 ? 1 : 0;

	if (g_scriptFrame[player] >= 0 && g_scriptFrame[player] < g_scriptCount[player])
	{
		const PlayerControl::Input frame = g_script[player][g_scriptFrame[player]];
		WriteStruct(out, frame.lever, frame.buttons);
		return;
	}

	if (player == dummy && (g_heldLever != 0 || g_heldButtons != 0 || g_tapFrames > 0))
	{
		uint8_t lever = g_heldLever;
		uint8_t buttons = g_heldButtons;

		if (g_tapFrames > 0)
		{
			if (g_tapLever != 0)
				lever = g_tapLever;

			buttons = static_cast<uint8_t>(buttons | g_tapButtons);
		}

		WriteStruct(out, lever, buttons);
		return;
	}

	if (g_mode != PlayerControl::Mode_Both || player == home)
		return;

	uint8_t buttons[12] = {};
	if (!ReadPadRecord(home, buttons))
		return;

	uint8_t* const bytes = static_cast<uint8_t*>(out);

	for (int i = 0; i < 12; ++i)
		bytes[0x08 + i] = buttons[i];

	uint8_t mask = 0;
	for (int bit = 0; bit < 4; ++bit)
	{
		if (buttons[g_buttonByte[bit]] != 0)
			mask = static_cast<uint8_t>(mask | (1u << bit));
	}

	const uintptr_t recorder = RvaToAddress(GameOffsets::kRecorderObject);
	if (recorder == 0)
		return;

	uint32_t live = 0;
	const uintptr_t at = recorder + GameOffsets::kRecorderLiveInput;

	if (TryReadDword(reinterpret_cast<const void*>(at), live))
	{
		const uint32_t sides = (live & 0xff00ff00u) | (mask << 16) | mask;
		TryWriteDword(reinterpret_cast<void*>(at), sides);
	}
}

}

void PlayerControl::OnPadFetched(void* out, int player)
{
	if (out == nullptr || player < 0 || player > 1 || !GameState::AllowsTrainingTools())
		return;

	if (g_calibrating)
	{
		if (player != GetHomeSide())
			return;

		uint8_t* const bytes = static_cast<uint8_t*>(out);

		bytes[0x04] = 0;
		for (int i = 0; i < 12; ++i)
			bytes[0x08 + i] = 0;

		if (g_calibratePhase == 0)
			bytes[0x08 + g_calibrateByte] = 1;

		return;
	}

	DriveFetched(out, player);
}

const char* PlayerControl::GetStatus()
{
	return g_status;
}
