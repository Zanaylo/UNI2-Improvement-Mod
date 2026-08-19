#include "Game/ReplayState.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/OnlineState.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr uintptr_t kBattleMode = 0x597948;
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
	{ "kBattleMode",          0x597948,  4, 0, false },
	{ "kSubMode",             0x59794c,  4, 0, false },
};

constexpr int kSignalCount = static_cast<int>(sizeof(g_signals) / sizeof(g_signals[0]));

constexpr int kNoisyChangesPerSecond = 8;

int g_changesThisSecond[kSignalCount] = {};
bool g_noisy[kSignalCount] = {};
DWORD g_windowStart = 0;

bool g_playing = false;
bool g_everFound = false;
bool g_sampled = false;
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

void SampleSignals()
{
	const DWORD now = GetTickCount();

	if (now - g_windowStart >= 1000)
	{
		g_windowStart = now;
		for (int i = 0; i < kSignalCount; ++i)
			g_changesThisSecond[i] = 0;
	}

	for (int i = 0; i < kSignalCount; ++i)
	{
		ReplayState::Signal& signal = g_signals[i];

		uint32_t value = 0;
		const bool read = ReadSignal(signal, value);

		if (g_sampled && read == signal.read && (!read || value == signal.value))
			continue;

		if (g_noisy[i])
		{
			signal.value = value;
			signal.read = read;
			continue;
		}

		if (++g_changesThisSecond[i] > kNoisyChangesPerSecond)
		{
			g_noisy[i] = true;
			signal.value = value;
			signal.read = read;

			LOG("ReplayState: %-20s changes several times a second, not a flag - dropped from the log",
				signal.name);
			continue;
		}

		if (!read)
			LOG("ReplayState: %-20s rva 0x%06x unreadable", signal.name, (unsigned)signal.rva);
		else if (!g_sampled || !signal.read)
			LOG("ReplayState: %-20s rva 0x%06x = 0x%08x", signal.name, (unsigned)signal.rva, value);
		else
			LOG("ReplayState: %-20s 0x%08x -> 0x%08x", signal.name, signal.value, value);

		signal.value = value;
		signal.read = read;
	}

	g_sampled = true;
}

}

void ReplayState::Update()
{
	SampleSignals();

	uint32_t mode = 0;
	const bool read = TryReadDword(reinterpret_cast<const void*>(RvaToAddress(kBattleMode)), mode);

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
			sprintf_s(g_status, "battle mode %u - %s", mode, ModeName(mode));
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

	outSignal = g_signals[index];
	return true;
}
