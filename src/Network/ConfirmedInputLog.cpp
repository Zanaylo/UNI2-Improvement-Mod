#include "Network/ConfirmedInputLog.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/NetLog.h"

#include <cstring>

namespace {

constexpr uint32_t kFirstValidSession = 0x10000;

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

}

ConfirmedInputLog::ConfirmedInputLog(int frames)
	: m_log(static_cast<size_t>(frames > 0 ? frames : 1))
{
}

void ConfirmedInputLog::Reset()
{
	m_session = 0;
	m_captured = -1;
	m_inputBytes = 0;
	m_gapLogged = false;
}

void ConfirmedInputLog::Capture(uint32_t session)
{
	if (session != m_session)
	{
		Reset();
		m_session = session;
	}

	if (session < kFirstValidSession)
		return;

	const int players = static_cast<int>(Dword(session + GameOffsets::kGgpoSyncPlayers));
	const int size = static_cast<int>(Dword(session + GameOffsets::kGgpoSyncInputSize));

	if (players < 1 || players > GameOffsets::kGgpoMostPlayers || size < 1 || players * size > kInputBytes)
		return;

	m_inputBytes = players * size;

	const int confirmed = static_cast<int>(Dword(session + GameOffsets::kGgpoLastConfirmedFrame));
	const int frames = static_cast<int>(m_log.size());

	for (int frame = m_captured + 1; frame <= confirmed; ++frame)
	{
		Entry& entry = m_log[static_cast<size_t>(frame % frames)];

		if (!ReadFrame(session, players, size, frame, entry))
		{
			if (!m_gapLogged)
				NetLog::Write("confirmed input log: frame %d was already gone from the game's input queue", frame);

			m_gapLogged = true;
			return;
		}

		m_captured = frame;
	}
}

int ConfirmedInputLog::Confirmed() const
{
	return m_captured;
}

int ConfirmedInputLog::InputBytes() const
{
	return m_inputBytes;
}

bool ConfirmedInputLog::Read(int frame, uint8_t* out) const
{
	const int frames = static_cast<int>(m_log.size());

	if (frame < 0 || frame > m_captured || m_captured - frame >= frames)
		return false;

	const Entry& entry = m_log[static_cast<size_t>(frame % frames)];

	if (entry.frame != frame)
		return false;

	memcpy(out, entry.bits, static_cast<size_t>(m_inputBytes));
	return true;
}

bool ConfirmedInputLog::ReadFrame(uint32_t session, int players, int size, int frame, Entry& out) const
{
	const uintptr_t queues = Dword(session + GameOffsets::kGgpoInputQueues);
	const uintptr_t status = Dword(session + GameOffsets::kGgpoLocalConnectStatus);

	if (queues < kFirstValidSession)
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
