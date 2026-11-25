#include "Game/Replays/ReplayState.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/OnlineState.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr uint32_t kBattleModeReplay = 1;

const char* ModeName(uint32_t mode)
{
	switch (mode)
	{
	case 0: return "single player";
	case 1: return "replay, CPU vs CPU or online";
	case 2: return "versus CPU";
	case 3: return "training";
	case 4: return "tutorial";
	case 5: return "mission";
	default: return "unknown";
	}
}

ReplayState::Signal g_signals[] =
{
	{ "CReplayFilerLayer*",   0x5b35cc,  4, 0, false },
	{ "replay menu state",    0x5b35c8,  4, 0, false },
	{ "CSceneReplayVSMenu*",  0x5b35b8,  4, 0, false },
	{ "watch flag fc",        0x5b91fc,  1, 0, false },
	{ "watch flag fd",        0x5b91fd,  1, 0, false },
	{ "watch flag fe",        0x5b91fe,  1, 0, false },
	{ "watch flag 44",        0x5b9244,  1, 0, false },
	{ "watch flag 45",        0x5b9245,  1, 0, false },
	{ "watch slot",           0x5b9260,  4, 0, false },
	{ "mode dword",           0x1a33be4, 4, 0, false },
	{ "beside mode",          0x1a34048, 4, 0, false },
	{ "kBattleMode",          GameOffsets::kBattleMode, 4, 0, false },
	{ "kSubMode",             GameOffsets::kSubMode, 4, 0, false },
};

constexpr int kSignalCount = static_cast<int>(sizeof(g_signals) / sizeof(g_signals[0]));

bool g_playing = false;
bool g_everFound = false;
char g_status[160] = "not read yet";

constexpr uint32_t kModeUnreadable = 0xffffffff;
uint32_t g_reportedMode = kModeUnreadable - 1;

bool ReadSignal(const ReplayState::Signal& signal, uint32_t& outValue)
{
	const void* address = reinterpret_cast<const void*>(RvaToAddress(signal.rva));

	if (signal.width == 1)
	{
		uint8_t byte = 0;
		if (!TryReadMemory(&byte, address, sizeof(byte)))
			return false;

		outValue = byte;
		return true;
	}

	return TryReadDword(address, outValue);
}

}

void ReplayState::Update()
{
	uint32_t mode = 0;
	const bool read = TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kBattleMode)), mode);

	const bool playing = read && mode == kBattleModeReplay && !OnlineState::IsOnline();
	const bool changed = playing != g_playing;

	g_playing = playing;

	if (playing)
		g_everFound = true;

	const uint32_t reported = read ? mode : kModeUnreadable;

	if (reported != g_reportedMode)
	{
		g_reportedMode = reported;

		if (read)
			sprintf_s(g_status, "battle mode %u (%s)", mode, ModeName(mode));
		else
			sprintf_s(g_status, "battle mode unreadable");
	}

	if (changed)
		LOG("ReplayState: %s", g_status);
}

bool ReplayState::IsPlaying()
{
	return g_playing;
}

bool ReplayState::IsDetectionReady()
{
	return g_everFound;
}

const char* ReplayState::GetStatusText()
{
	return g_status;
}

int ReplayState::GetSignalCount()
{
	return kSignalCount;
}

bool ReplayState::GetSignal(int index, Signal& outSignal)
{
	if (index < 0 || index >= kSignalCount)
		return false;

	Signal& signal = g_signals[index];
	uint32_t value = 0;
	signal.read = ReadSignal(signal, value);
	signal.value = value;
	outSignal = signal;
	return true;
}
