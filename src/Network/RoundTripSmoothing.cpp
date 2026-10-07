#include "Network/RoundTripSmoothing.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/RoundTripFilter.h"

#include <Windows.h>

#include <cstring>

static volatile LONG g_formulaValue = 0;

enum : int
{
	kRoundTripOffset = static_cast<int>(GameOffsets::kGgpoEndpointRoundTrip)
};

__declspec(naked) static void SmoothedRoundTrip()
{
	__asm
	{
		mov eax, g_formulaValue
		test eax, eax
		jz raw
		cmp dword ptr [esi + kRoundTripOffset], 0
		jne done
	raw:
		mov eax, dword ptr [esi + kRoundTripOffset]
	done:
		ret
	}
}

namespace {

constexpr double kPerFrameWeight = 1.0 / 180.0;
constexpr int kMostPlausiblePingMs = 2000;
constexpr size_t kSiteBytes = 6;
constexpr uint8_t kCallRel32 = 0xE8;
constexpr uint8_t kNop = 0x90;
constexpr size_t kCallBytes = 5;
constexpr uint8_t kMeasuredRead[kSiteBytes] = { 0x8B, 0x86, 0x60, 0x08, 0x00, 0x00 };

RoundTripFilter g_filter(kPerFrameWeight);
bool g_installed = false;
uint32_t g_session = 0;
int g_loggedFrames = -1;

uint8_t* SiteAddress()
{
	const uintptr_t site = CodeSignatures::Address(GameOffsets::kSiteAdvantageRoundTrip);

	return IsAddressInGameModule(site) ? reinterpret_cast<uint8_t*>(site) : nullptr;
}

bool StillAsMeasured(const uint8_t* site)
{
	uint8_t current[kSiteBytes] = {};

	return TryReadMemory(current, site, kSiteBytes) && memcmp(current, kMeasuredRead, kSiteBytes) == 0;
}

bool RedirectToStub(uint8_t* site)
{
	uint8_t call[kSiteBytes] = { kCallRel32, 0, 0, 0, 0, kNop };
	const int32_t relative = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&SmoothedRoundTrip) -
		(reinterpret_cast<uintptr_t>(site) + kCallBytes));

	memcpy(call + 1, &relative, sizeof(relative));
	return WriteCodeBytes(site, call, kSiteBytes);
}

void Forget()
{
	g_filter.Reset();
	g_loggedFrames = -1;
	InterlockedExchange(&g_formulaValue, 0);
}

void LogFramesChange(int rawMs)
{
	if (!g_filter.IsPrimed() || g_filter.Frames() == g_loggedFrames)
		return;

	g_loggedFrames = g_filter.Frames();
	NetLog::Write("time sync round trip: %d frame(s), smoothed %.1f ms, raw %d ms", g_loggedFrames,
		g_filter.Smoothed(), rawMs);
}

}

bool RoundTripSmoothing::Install()
{
	if (!g_modVals.smoothRoundTrip)
	{
		LOG("RoundTripSmoothing: off in the ini, the time sync reads GGPO's raw round trip");
		return false;
	}

	uint8_t* const site = SiteAddress();

	if (site == nullptr || !StillAsMeasured(site))
	{
		LOG("RoundTripSmoothing: the advantage formula is not the measured 1.40 bytes, left alone");
		return false;
	}

	if (!RedirectToStub(site))
	{
		LOG("RoundTripSmoothing: could not write the call, left alone");
		return false;
	}

	g_installed = true;
	LOG("RoundTripSmoothing: the time sync now reads a smoothed, rounded round trip at 0x%p", static_cast<void*>(site));
	return true;
}

void RoundTripSmoothing::OnFrame()
{
	if (!g_installed)
		return;

	const NetLink::Snapshot& link = NetLink::Current();

	if (link.session != g_session)
	{
		g_session = link.session;
		Forget();
	}

	if (!link.hasPeer || link.peer.ping > kMostPlausiblePingMs)
		return;

	g_filter.Update(link.peer.ping);
	InterlockedExchange(&g_formulaValue, g_filter.FormulaValue());
	LogFramesChange(link.peer.ping);
}

bool RoundTripSmoothing::IsInstalled()
{
	return g_installed;
}

bool RoundTripSmoothing::IsPrimed()
{
	return g_installed && g_filter.IsPrimed();
}

double RoundTripSmoothing::SmoothedMs()
{
	return g_filter.Smoothed();
}

int RoundTripSmoothing::Frames()
{
	return g_filter.Frames();
}
