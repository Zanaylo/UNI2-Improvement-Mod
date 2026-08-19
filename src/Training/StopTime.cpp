#include "Training/StopTime.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

namespace {

using SetStopTimeAll_t = void(__fastcall*)(void* battleObject, void* unusedEdx, int frames);

constexpr uintptr_t kQueueBegin = 0x04;
constexpr uintptr_t kQueueEnd = 0x08;
constexpr uintptr_t kQueueCapacity = 0x0c;
constexpr uintptr_t kMessageSize = 0x14;
constexpr uintptr_t kMessageTargetId = 0x04;
constexpr uintptr_t kMessageFrames = 0x0c;
constexpr uint16_t kTargetAllCharacters = 0xfffe;

constexpr int kMaxQueueEntries = 256;

SetStopTimeAll_t g_setStopTimeAll = nullptr;
void* g_battleObject = nullptr;
bool g_initialized = false;

uint64_t g_applyCount = 0;
uint64_t g_rejectCount = 0;
const char* g_lastRejectReason = "";

volatile long g_requestedFrames = -1;

struct QueueView
{
	uintptr_t begin;
	uintptr_t end;
	uintptr_t capacity;
	int count;
};

bool ReadQueue(QueueView& out)
{
	if (g_battleObject == nullptr)
		return false;

	uint32_t fields[3] = {};
	if (!TryReadMemory(fields, reinterpret_cast<const uint8_t*>(g_battleObject) + kQueueBegin,
		sizeof(fields)))
	{
		return false;
	}

	out.begin = fields[0];
	out.end = fields[1];
	out.capacity = fields[2];

	if (out.begin == 0 || out.end < out.begin || out.capacity < out.end)
		return false;

	const uintptr_t span = out.end - out.begin;
	if (span % kMessageSize != 0)
		return false;

	out.count = static_cast<int>(span / kMessageSize);
	return out.count <= kMaxQueueEntries;
}

bool CanApply()
{
	if (g_setStopTimeAll == nullptr || g_battleObject == nullptr)
	{
		g_lastRejectReason = "not initialised";
		return false;
	}

	QueueView view = {};
	if (!ReadQueue(view))
	{
		g_lastRejectReason = "queue not built or out of range";
		return false;
	}

	g_lastRejectReason = "";
	return true;
}

}

bool StopTime::Initialize()
{
	if (g_initialized)
		return IsAvailable();

	g_initialized = true;

	g_setStopTimeAll = reinterpret_cast<SetStopTimeAll_t>(RvaToAddress(GameOffsets::kFnSetStopTime));

	g_battleObject = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kBattleObject));

	LOG("StopTime: setter at 0x%p, battle object at 0x%p", (void*)g_setStopTimeAll, g_battleObject);
	return IsAvailable();
}

bool StopTime::IsAvailable()
{
	return g_setStopTimeAll != nullptr && g_battleObject != nullptr;
}

bool StopTime::ApplyAll(int frames)
{
	if (!CanApply())
	{
		++g_rejectCount;
		return false;
	}

	g_setStopTimeAll(g_battleObject, nullptr, frames);
	++g_applyCount;
	return true;
}

void StopTime::RequestOneShot(int frames)
{
	InterlockedExchange(&g_requestedFrames, frames < 0 ? 0 : frames);
}

void StopTime::ServiceRequest()
{
	const long frames = InterlockedExchange(&g_requestedFrames, -1);
	if (frames < 0)
		return;

	ApplyAll(static_cast<int>(frames));
}

uint64_t StopTime::GetApplyCount()
{
	return g_applyCount;
}

uint64_t StopTime::GetRejectCount()
{
	return g_rejectCount;
}

const char* StopTime::GetLastRejectReason()
{
	return g_lastRejectReason;
}

bool StopTime::ReadQueueState(int& outCount, int& outCapacity)
{
	QueueView view = {};
	if (!ReadQueue(view))
		return false;

	outCount = view.count;
	outCapacity = static_cast<int>((view.capacity - view.begin) / kMessageSize);
	return true;
}

bool StopTime::ReadQueuedFrames(int& outFrames)
{
	QueueView view = {};
	if (!ReadQueue(view))
		return false;

	for (int i = 0; i < view.count; ++i)
	{
		const uintptr_t message = view.begin + static_cast<uintptr_t>(i) * kMessageSize;

		uint16_t targetId = 0;
		if (!TryReadMemory(&targetId, reinterpret_cast<const void*>(message + kMessageTargetId),
			sizeof(targetId)))
		{
			return false;
		}

		if (targetId != kTargetAllCharacters)
			continue;

		int32_t frames = 0;
		if (!TryReadMemory(&frames, reinterpret_cast<const void*>(message + kMessageFrames),
			sizeof(frames)))
		{
			return false;
		}

		outFrames = frames;
		return true;
	}

	return false;
}
