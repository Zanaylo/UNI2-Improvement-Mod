#include "Network/SpectatorCatchUp.h"

#include "Core/Config/interfaces.h"
#include "Core/utils.h"
#include "Game/Display/FrameWaitSkip.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/CatchUpRule.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/SpectatorBacklog.h"
#include "Network/SpectatorStore.h"

#include <cstdint>

namespace {

constexpr int kStartFromFrames = 8;
constexpr int kStopAtFrames = 2;
constexpr int kSlots = GameOffsets::kSpectatorInputSlots;

struct RingSlot
{
	int32_t frame;
	uint8_t body[GameOffsets::kSpectatorInputBytes - sizeof(int32_t)];
};

static_assert(sizeof(RingSlot) == GameOffsets::kSpectatorInputBytes, "a ring slot is one GGPO input");

CatchUpRule g_rule(kStartFromFrames, kStopAtFrames);
uint32_t g_session = 0;
bool g_lappedReported = false;
RingSlot g_ring[kSlots] = {};
int32_t g_frames[kSlots] = {};

bool ReadField(uintptr_t backend, uintptr_t offset, void* out, size_t size)
{
	return TryReadMemory(out, reinterpret_cast<const void*>(backend + offset), size);
}

int ReadNextFrame(uintptr_t backend)
{
	int32_t next = 0;
	ReadField(backend, GameOffsets::kSpectatorNextFrame, &next, sizeof(next));

	return next;
}

bool ReadRingBacklog(uintptr_t backend, int next, SpectatorBacklog::Reading& out)
{
	if (!ReadField(backend, GameOffsets::kSpectatorInputs, g_ring, sizeof(g_ring)))
		return false;

	for (int i = 0; i < kSlots; ++i)
		g_frames[i] = g_ring[i].frame;

	out = SpectatorBacklog::Measure(g_frames, kSlots, next);
	return true;
}

bool ReadBacklog(uintptr_t backend, SpectatorBacklog::Reading& out)
{
	uint8_t synchronizing = 0;

	if (!ReadField(backend, GameOffsets::kSpectatorSynchronizing, &synchronizing, sizeof(synchronizing)) ||
		synchronizing != 0)
	{
		return false;
	}

	const int next = ReadNextFrame(backend);

	if (SpectatorStore::IsActive())
	{
		out = { SpectatorStore::ReadyFrom(next), false };
		return true;
	}

	return ReadRingBacklog(backend, next, out);
}

void LogRunEnd(const char* why)
{
	NetLog::Write("spectator catch-up: %s after %d hurried frame(s), largest backlog %d", why,
		g_rule.LastRunFrames(), g_rule.LargestBacklog());
}

void StandDown(const char* why)
{
	if (!g_rule.IsCatchingUp())
		return;

	g_rule.Reset();
	LogRunEnd(why);
}

void ReleaseStore()
{
	if (SpectatorStore::IsHeld())
		return;

	SpectatorStore::SetActive(false);
}

void UseStore(uintptr_t backend)
{
	if (SpectatorStore::IsActive())
		return;

	if (!SpectatorStore::IsHeld())
		SpectatorStore::StartAt(ReadNextFrame(backend));

	SpectatorStore::SetActive(true);
}

void Follow(uint32_t session)
{
	if (session == g_session)
		return;

	StandDown("the spectator session ended");
	ReleaseStore();
	g_session = session;
	g_lappedReported = false;
}

void ReportLapped()
{
	if (g_lappedReported)
		return;

	g_lappedReported = true;
	NetLog::Write("spectator catch-up: the spectator fell %d or more frames behind and the game cannot recover it",
		kSlots);
}

bool IsWatchingARoomMatch(const NetLink::Snapshot& link)
{
	return g_modVals.spectatorCatchUp && link.backend == NetLink::Backend_Spectator && FrameWaitSkip::IsMeasured();
}

}

void SpectatorCatchUp::Update()
{
	const NetLink::Snapshot& link = NetLink::Current();

	if (!IsWatchingARoomMatch(link))
	{
		StandDown("stopped");
		ReleaseStore();
		g_session = 0;
		return;
	}

	Follow(link.session);
	UseStore(link.session);

	SpectatorBacklog::Reading reading = {};

	if (!ReadBacklog(link.session, reading))
	{
		StandDown("the spectator session is not running");
		return;
	}

	if (reading.lapped)
	{
		ReportLapped();
		StandDown("gave up");
		return;
	}

	const bool wasCatchingUp = g_rule.IsCatchingUp();

	if (!g_rule.Update(reading.ready))
	{
		if (wasCatchingUp)
			LogRunEnd("caught up");

		return;
	}

	if (!wasCatchingUp)
		NetLog::Write("spectator catch-up: %d frame(s) behind the players, running without the frame wait", reading.ready);

	FrameWaitSkip::SkipNextWait();
}

bool SpectatorCatchUp::IsCatchingUp()
{
	return g_rule.IsCatchingUp();
}
