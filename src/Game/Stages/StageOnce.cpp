#include "Game/Stages/StageOnce.h"

#include "Core/logger.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kMaxNodes = 8192;
constexpr size_t kAnimeOwner = 0x14;
constexpr size_t kAnimeBegin = 0x2c;
constexpr size_t kAnimeEnd = 0x30;
constexpr size_t kFramesBegin = 0x08;
constexpr size_t kFramesEnd = 0x0c;
constexpr int kMatrixShift = 6;

uint32_t g_last[kMaxNodes] = {};
volatile long g_held = 0;
volatile long g_calls = 0;
volatile long g_clamps = 0;
char g_status[160] = "no stage has asked for a one-shot animation yet";

template <typename T>
T At(const uint8_t* base, size_t offset)
{
	T value;
	memcpy(&value, base + offset, sizeof(T));

	return value;
}

uint32_t Frames(const uint8_t* node, int index)
{
	const uint8_t* const owner = At<const uint8_t*>(node, kAnimeOwner);

	if (owner == nullptr)
		return 0;

	const uint8_t* const* const begin = At<const uint8_t* const*>(owner, kAnimeBegin);
	const uint8_t* const* const end = At<const uint8_t* const*>(owner, kAnimeEnd);

	if (begin == nullptr || index >= end - begin || begin[index] == nullptr)
		return 0;

	const uint8_t* const entry = begin[index];

	return static_cast<uint32_t>((At<intptr_t>(entry, kFramesEnd) - At<intptr_t>(entry, kFramesBegin))
		>> kMatrixShift);
}

}

void StageOnce::Hold(const std::vector<int>& pairs)
{
	LOG("StageOnce: the sampler ran %ld time(s) and held a node %ld time(s) on the last stage",
		InterlockedExchange(&g_calls, 0), InterlockedExchange(&g_clamps, 0));

	InterlockedExchange(&g_held, 0);
	memset(g_last, 0, sizeof(g_last));

	int held = 0;

	for (size_t i = 0; i + 1 < pairs.size(); i += 2)
	{
		const int node = pairs[i];
		const int frames = pairs[i + 1];

		if (node < 0 || node >= kMaxNodes || frames < 2)
			continue;

		g_last[node] = static_cast<uint32_t>(frames - 1);
		++held;
	}

	sprintf_s(g_status, "%d node(s) play their animation once and hold the last frame", held);

	if (held > 0)
		LOG("StageOnce: %s", g_status);

	InterlockedExchange(&g_held, held);
}

uint32_t StageOnce::Frame(const uint8_t* node, int index, uint32_t frame)
{
	InterlockedIncrement(&g_calls);

	if (InterlockedCompareExchange(&g_held, 0, 0) == 0 || index < 0 || index >= kMaxNodes || g_last[index] == 0
		|| frame <= g_last[index] || Frames(node, index) != g_last[index] + 1)
	{
		return frame;
	}

	InterlockedIncrement(&g_clamps);

	return g_last[index];
}

int StageOnce::Held()
{
	return static_cast<int>(InterlockedCompareExchange(&g_held, 0, 0));
}

const char* StageOnce::StatusText()
{
	return g_status;
}
