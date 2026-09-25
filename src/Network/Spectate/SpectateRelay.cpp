#include "Network/Spectate/SpectateRelay.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/NetLog.h"

#include <cstring>

namespace {

struct Entry
{
	int32_t frame;
	uint8_t bits[SpectateWire::kInputBytes];
};

Entry g_log[SpectateRelay::kLogFrames] = {};
uint32_t g_session = 0;
int g_captured = -1;
int g_inputBytes = 0;
bool g_gapLogged = false;

uint32_t Dword(uintptr_t address)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(address), value);

	return value;
}

bool Disconnected(uintptr_t status, int player, int frame)
{
	const uint32_t value = Dword(status + static_cast<uintptr_t>(player) * sizeof(uint32_t));

	return (value & 1) != 0 && frame > static_cast<int>(value >> 1);
}

bool ReadPlayer(uintptr_t queues, uintptr_t status, int player, int frame, int size, uint8_t* out)
{
	if (status != 0 && Disconnected(status, player, frame))
	{
		memset(out, 0, static_cast<size_t>(size));
		return true;
	}

	const uintptr_t queue = queues + static_cast<uintptr_t>(player) * GameOffsets::kGgpoInputQueueStride;
	const uintptr_t slot = queue + GameOffsets::kGgpoQueueInputs +
		static_cast<uintptr_t>(frame & (GameOffsets::kGgpoQueueSlots - 1)) * GameOffsets::kGgpoQueueInputStride;

	if (static_cast<int>(Dword(slot)) != frame)
		return false;

	return TryReadMemory(out, reinterpret_cast<const void*>(slot + GameOffsets::kGgpoInputBits), static_cast<size_t>(size));
}

bool ReadFrame(uint32_t session, int players, int size, int frame, Entry& out)
{
	const uintptr_t queues = Dword(session + GameOffsets::kGgpoInputQueues);
	const uintptr_t status = Dword(session + GameOffsets::kGgpoLocalConnectStatus);

	if (queues < 0x10000)
		return false;

	out.frame = frame;
	memset(out.bits, 0, sizeof(out.bits));

	for (int player = 0; player < players; ++player)
	{
		if (!ReadPlayer(queues, status, player, frame, size, out.bits + player * size))
			return false;
	}

	return true;
}

}

void SpectateRelay::Reset()
{
	g_session = 0;
	g_captured = -1;
	g_inputBytes = 0;
	g_gapLogged = false;
}

void SpectateRelay::Capture(uint32_t session)
{
	if (session != g_session)
	{
		Reset();
		g_session = session;
	}

	if (session < 0x10000)
		return;

	const int players = static_cast<int>(Dword(session + GameOffsets::kGgpoSyncPlayers));
	const int size = static_cast<int>(Dword(session + GameOffsets::kGgpoSyncInputSize));

	if (players < 1 || players > GameOffsets::kGgpoMostPlayers || size < 1 || players * size > SpectateWire::kInputBytes)
		return;

	g_inputBytes = players * size;

	const int confirmed = static_cast<int>(Dword(session + GameOffsets::kGgpoLastConfirmedFrame));

	for (int frame = g_captured + 1; frame <= confirmed; ++frame)
	{
		Entry& entry = g_log[frame % kLogFrames];

		if (!ReadFrame(session, players, size, frame, entry))
		{
			if (!g_gapLogged)
				NetLog::Write("spectate relay: frame %d was already gone from the game's input queue", frame);

			g_gapLogged = true;
			return;
		}

		g_captured = frame;
	}
}

int SpectateRelay::Confirmed()
{
	return g_captured;
}

int SpectateRelay::InputBytes()
{
	return g_inputBytes;
}

bool SpectateRelay::Read(int frame, uint8_t* out)
{
	if (frame < 0 || frame > g_captured || g_captured - frame >= kLogFrames)
		return false;

	const Entry& entry = g_log[frame % kLogFrames];

	if (entry.frame != frame)
		return false;

	memcpy(out, entry.bits, static_cast<size_t>(g_inputBytes));
	return true;
}
