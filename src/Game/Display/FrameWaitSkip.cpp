#include "Game/Display/FrameWaitSkip.h"

#include "Core/CodeFingerprint.h"
#include "Core/MeasuredCode.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"

#include <cstdint>

namespace {

constexpr uint32_t kSkipTheWait = 1;

const CodeFingerprint kSkipFlagCode[] = {
	{ GameOffsets::kSiteFrameWaitSkipOnceRead, { 0x83, 0x3D }, 2, GameOffsets::kFrameWaitSkipOnce, { 0x00 }, 1 },
	{ GameOffsets::kSiteFrameWaitSkipOnceClear, { 0xC7, 0x05 }, 2, GameOffsets::kFrameWaitSkipOnce,
		{ 0x00, 0x00, 0x00, 0x00 }, 4 },
};

enum class Readiness
{
	Unchecked,
	Ready,
	Refused,
};

Readiness g_readiness = Readiness::Unchecked;

}

bool FrameWaitSkip::IsMeasured()
{
	if (g_readiness != Readiness::Unchecked)
		return g_readiness == Readiness::Ready;

	const int mismatch = MeasuredCode::FirstMismatch(kSkipFlagCode);
	g_readiness = mismatch == MeasuredCode::kAllMatch ? Readiness::Ready : Readiness::Refused;

	if (g_readiness == Readiness::Refused)
	{
		LOG("FrameWaitSkip: rva 0x%x is not the measured 1.40 code, catch-up stays off",
			static_cast<unsigned>(kSkipFlagCode[mismatch].siteRva));
	}

	return g_readiness == Readiness::Ready;
}

void FrameWaitSkip::SkipNextWait()
{
	if (!IsMeasured())
		return;

	TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFrameWaitSkipOnce)), &kSkipTheWait,
		sizeof(kSkipTheWait));
}
