#include "Game/Lobby/IrHider.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/GameHook.h"

#include <cstdio>
#include <cstring>

namespace {

typedef void(__cdecl* DrawIrNumberFn)();

constexpr size_t kMaxBranchBytes = 2;

constexpr uint8_t kNop = 0x90;
constexpr uint8_t kJumpNear = 0xe9;
constexpr uint8_t kJumpShort = 0xeb;
constexpr uint8_t kJumpIfEqualShort = 0x74;
constexpr uint8_t kTwoByteOpcode = 0x0f;
constexpr uint8_t kJumpIfNotEqualNear = 0x85;

struct BranchSite
{
	uintptr_t rva;
	const char* label;
	size_t size;
	uint8_t shown[kMaxBranchBytes];
	uint8_t hidden[kMaxBranchBytes];
};

const BranchSite kSites[] = {
	{ GameOffsets::kSiteBattlePlateIr, "battle name plate", 2,
		{ kTwoByteOpcode, kJumpIfNotEqualNear }, { kNop, kJumpNear } },
	{ GameOffsets::kSitePlayerCardIrText, "player card text", 1, { kJumpIfEqualShort }, { kJumpShort } },
	{ GameOffsets::kSiteStatsIrHeader, "player stats header", 1, { kJumpIfEqualShort }, { kJumpShort } },
	{ GameOffsets::kSiteStatsIrHeaderAlt, "player stats header, second layout", 1,
		{ kJumpIfEqualShort }, { kJumpShort } },
};

constexpr int kSiteCount = static_cast<int>(sizeof(kSites) / sizeof(kSites[0]));

GameHook<DrawIrNumberFn> g_drawIrNumberHook("DrawIrNumber");

uint8_t* g_siteCode[kSiteCount] = {};
int g_sitesReady = 0;
bool g_installed = false;
bool g_enabled = false;
char g_status[192] = "not installed yet";

void __cdecl SkipIrNumber()
{
}

bool Holds(const uint8_t* code, const uint8_t* bytes, size_t size)
{
	uint8_t current[kMaxBranchBytes] = {};

	return TryReadMemory(current, code, size) && memcmp(current, bytes, size) == 0;
}

uint8_t* LocateSite(const BranchSite& site)
{
	const uintptr_t address = CodeSignatures::Address(site.rva);

	if (!IsAddressInGameModule(address))
		return nullptr;

	uint8_t* const code = reinterpret_cast<uint8_t*>(address);

	return Holds(code, site.shown, site.size) ? code : nullptr;
}

void LocateSites()
{
	for (int i = 0; i < kSiteCount; ++i)
	{
		g_siteCode[i] = LocateSite(kSites[i]);

		if (g_siteCode[i] == nullptr)
		{
			LOG("IrHider: the %s branch is not where this game version expects it", kSites[i].label);
			continue;
		}

		++g_sitesReady;
	}
}

void RewriteSite(int index, bool hide)
{
	uint8_t* const code = g_siteCode[index];

	if (code == nullptr)
		return;

	const BranchSite& site = kSites[index];
	const uint8_t* const wanted = hide ? site.hidden : site.shown;

	if (Holds(code, wanted, site.size))
		return;

	if (!WriteCodeBytes(code, wanted, site.size))
		LOG("IrHider: could not rewrite the %s branch", site.label);
}

void Summarise()
{
	if (!IrHider::IsAvailable())
	{
		strncpy_s(g_status, "the game's IR drawing code is not where this game version expects it",
			_TRUNCATE);
		return;
	}

	if (!g_enabled)
	{
		strncpy_s(g_status, "off, IR is shown as the game draws it", _TRUNCATE);
		return;
	}

	sprintf_s(g_status, "on, IR numbers are %s and %d of %d IR labels are hidden",
		g_drawIrNumberHook.IsLive() ? "hidden" : "still shown", g_sitesReady, kSiteCount);
}

void Apply()
{
	if (!g_installed)
		return;

	for (int i = 0; i < kSiteCount; ++i)
		RewriteSite(i, g_enabled);

	g_drawIrNumberHook.SetEnabled(g_enabled);
	Summarise();
}

}

bool IrHider::Install()
{
	if (g_installed)
		return IsAvailable();

	g_drawIrNumberHook.CreateRva(GameOffsets::kFnDrawIrNumber, &SkipIrNumber);
	LocateSites();

	g_installed = true;
	Apply();

	LOG("IrHider: %s", g_status);
	return IsAvailable();
}

bool IrHider::IsAvailable()
{
	return g_installed && (g_drawIrNumberHook.IsLive() || g_sitesReady > 0);
}

bool IrHider::IsEnabled()
{
	return g_enabled;
}

void IrHider::SetEnabled(bool enabled)
{
	if (enabled == g_enabled)
		return;

	g_enabled = enabled;
	Apply();
}

long IrHider::NumbersHidden()
{
	return g_drawIrNumberHook.Calls();
}

const char* IrHider::StatusText()
{
	return g_status;
}
