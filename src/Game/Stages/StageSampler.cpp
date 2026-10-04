#include "Game/Stages/StageSampler.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Stages/StageKick.h"
#include "Game/Stages/StageOnce.h"
#include "Hooks/GameHook.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>

namespace {

constexpr size_t kAnimeIndex = 0x08;

using Sample_t = void(__fastcall*)(void*, void*, const uint8_t*, uint32_t, const int*);

GameHook<Sample_t> g_sampleHook("FbxAnimeSample");
volatile long g_clock = 0;

void __fastcall HookedSample(void* scene, void* unused, const uint8_t* node, uint32_t frame, const int* mode)
{
	if (node == nullptr || mode == nullptr || *mode != 0)
	{
		g_sampleHook.Original()(scene, unused, node, frame, mode);
		return;
	}

	InterlockedExchange(&g_clock, static_cast<long>(frame));

	int index = 0;
	memcpy(&index, node + kAnimeIndex, sizeof(index));

	const bool kicked = StageKick::Frame(index, frame);

	if (!kicked)
		frame = StageOnce::Frame(node, index, frame);

	g_sampleHook.Original()(scene, unused, node, frame, mode);

	if (kicked)
		StageKick::Place(scene, index);
}

}

bool StageSampler::Ticking()
{
	return g_sampleHook.IsLive();
}

uint32_t StageSampler::Clock()
{
	return static_cast<uint32_t>(InterlockedCompareExchange(&g_clock, 0, 0));
}

bool StageSampler::Initialize()
{
	const uintptr_t address = CodeSignatures::Address(GameOffsets::kFnFbxAnimeSample);

	if (!IsAddressInGameModule(address))
	{
		LOG("StageSampler: the stage animation sampler is not where this game version expects it");
		return false;
	}

	if (!g_sampleHook.Install(reinterpret_cast<void*>(address), &HookedSample))
	{
		LOG("StageSampler: the stage animation sampler could not be hooked");
		return false;
	}

	return true;
}
