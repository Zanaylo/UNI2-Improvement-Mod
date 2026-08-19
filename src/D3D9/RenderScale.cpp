#include "D3D9/RenderScale.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

// Two writes into the size globals plus two literals in the setup, per axis.
constexpr int kExpectedSites = 4;
constexpr int kTargetCount = 5;

struct Site
{
	uintptr_t address;
	uint32_t original;
};

Site g_widthSites[8] = {};
int g_widthCount = 0;

Site g_heightSites[8] = {};
int g_heightCount = 0;

bool g_installed = false;
bool g_installTried = false;
bool g_applied = false;
int g_appliedPercent = 100;

// The game makes render targets for other things too - a 256x256 one turns up during a match - so a
// single last-one-wins slot reported whichever happened to be newest. Keep a small table and report
// the biggest, which is the full screen chain this feature is about.
struct Observed
{
	unsigned width;
	unsigned height;
	int count;
};

constexpr int kObservedSlots = 8;

Observed g_observed[kObservedSlots] = {};
int g_observedUsed = 0;
int g_observedFailures = 0;

char g_status[256] = "not installed";

bool ReadDword(uintptr_t address, uint32_t& out)
{
	return TryReadDword(reinterpret_cast<const void*>(address), out);
}

bool WriteCodeDword(uintptr_t address, uint32_t value)
{
	DWORD previous = 0;
	if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), PAGE_EXECUTE_READWRITE,
		&previous))
	{
		return false;
	}

	memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));

	DWORD restored = 0;
	VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), previous, &restored);
	FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), sizeof(value));
	return true;
}

// Each site is checked against the exact instruction it is expected to be before anything is
// written, the way LoopSleep used to check its byte: the opcode, the global it addresses, and the
// value it currently holds. A game patch that moves any of them fails the check and the whole
// feature refuses rather than writing into the middle of something else.
bool ValidateGlobalWrite(uintptr_t rva, uintptr_t globalRva, uint32_t expected, Site& out)
{
	const uintptr_t address = RvaToAddress(rva);

	uint16_t opcode = 0;
	if (!TryReadMemory(&opcode, reinterpret_cast<const void*>(address), sizeof(opcode)))
		return false;

	if (opcode != 0x05c7)
		return false;

	uint32_t operand = 0;
	if (!ReadDword(address + 2, operand) || operand != static_cast<uint32_t>(RvaToAddress(globalRva)))
		return false;

	uint32_t immediate = 0;
	if (!ReadDword(address + 6, immediate) || immediate != expected)
		return false;

	out.address = address + 6;
	out.original = immediate;
	return true;
}

bool ValidateLiteral(uintptr_t rva, uint8_t opcode, uint32_t expected, Site& out)
{
	const uintptr_t address = RvaToAddress(rva);

	uint8_t found = 0;
	if (!TryReadMemory(&found, reinterpret_cast<const void*>(address), sizeof(found)))
		return false;

	if (found != opcode)
		return false;

	uint32_t immediate = 0;
	if (!ReadDword(address + 1, immediate) || immediate != expected)
		return false;

	out.address = address + 1;
	out.original = immediate;
	return true;
}

void SetStatus(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(g_status, sizeof(g_status), format, args);
	va_end(args);
}

int ClampPercent(int percent)
{
	if (percent < 25)
		return 25;
	if (percent > 400)
		return 400;

	return percent;
}

bool WriteAll(int width, int height)
{
	bool ok = true;

	for (int i = 0; i < g_widthCount; ++i)
		ok = WriteCodeDword(g_widthSites[i].address, static_cast<uint32_t>(width)) && ok;

	for (int i = 0; i < g_heightCount; ++i)
		ok = WriteCodeDword(g_heightSites[i].address, static_cast<uint32_t>(height)) && ok;

	return ok;
}

}

bool RenderScale::Install()
{
	if (g_installed)
		return true;

	// The overlay asks every frame it is open; validating and logging the refusal once is enough.
	if (g_installTried)
		return false;

	g_installTried = true;

	g_widthCount = 0;
	g_heightCount = 0;

	for (const uintptr_t rva : GameOffsets::kRenderSizeWidthWrites)
	{
		if (ValidateGlobalWrite(rva, GameOffsets::kRenderTargetWidth, kBaseWidth,
			g_widthSites[g_widthCount]))
		{
			++g_widthCount;
		}
	}

	for (const uintptr_t rva : GameOffsets::kRenderSizeHeightWrites)
	{
		if (ValidateGlobalWrite(rva, GameOffsets::kRenderTargetHeight, kBaseHeight,
			g_heightSites[g_heightCount]))
		{
			++g_heightCount;
		}
	}

	for (const uintptr_t rva : GameOffsets::kRenderSizeWidthLiterals)
	{
		if (ValidateLiteral(rva, 0xba, kBaseWidth, g_widthSites[g_widthCount]))
			++g_widthCount;
	}

	for (const uintptr_t rva : GameOffsets::kRenderSizeHeightLiterals)
	{
		if (ValidateLiteral(rva, 0x68, kBaseHeight, g_heightSites[g_heightCount]))
			++g_heightCount;
	}

	if (g_widthCount != kExpectedSites || g_heightCount != kExpectedSites)
	{
		SetStatus("%d of %d width sites and %d of %d height sites read as expected - refusing to "
			"touch it", g_widthCount, kExpectedSites, g_heightCount, kExpectedSites);
		LOG("[RenderScale] %s", g_status);
		g_widthCount = 0;
		g_heightCount = 0;
		return false;
	}

	g_installed = true;
	SetStatus("%dx%d as the game ships", kBaseWidth, kBaseHeight);
	LOG("[RenderScale] all %d sites validated, %dx%d as shipped", g_widthCount + g_heightCount,
		kBaseWidth, kBaseHeight);
	return true;
}

void RenderScale::SizeForPercent(int percent, int& outWidth, int& outHeight)
{
	const int clamped = ClampPercent(percent);

	outWidth = kBaseWidth * clamped / 100;
	outHeight = kBaseHeight * clamped / 100;

	outWidth = (outWidth + 3) & ~3;
	outHeight = (outHeight + 3) & ~3;
}

double RenderScale::EstimateMegabytes(int percent)
{
	int width = 0;
	int height = 0;
	SizeForPercent(percent, width, height);

	return static_cast<double>(width) * height * 4.0 * kTargetCount / (1024.0 * 1024.0);
}

// The game is a 32 bit process, so the ceiling is the address space rather than the card. Five full
// screen targets at 300% is over 150 MB on top of everything the game already holds.
bool RenderScale::IsAffordable(int percent, const char*& outReason)
{
	outReason = "";

	if (percent <= 100)
		return true;

	const double megabytes = EstimateMegabytes(percent) - EstimateMegabytes(100);

	MEMORYSTATUSEX memory = {};
	memory.dwLength = sizeof(memory);

	if (GlobalMemoryStatusEx(&memory))
	{
		const double freeMegabytes = static_cast<double>(memory.ullAvailVirtual) / (1024.0 * 1024.0);

		if (megabytes > freeMegabytes / 3.0)
		{
			outReason = "not enough address space left in a 32 bit process";
			return false;
		}
	}

	if (megabytes > static_cast<double>(g_modVals.internalResolutionBudgetMb))
	{
		outReason = "over the memory budget on the Graphics tab";
		return false;
	}

	return true;
}

void RenderScale::Apply()
{
	const int percent = ClampPercent(g_modVals.internalResolutionPercent);

	if (percent != 100 && !g_installed && !Install())
		return;

	if (!g_installed)
		return;

	const char* reason = "";
	if (!IsAffordable(percent, reason))
	{
		SetStatus("%d%% refused: %s", percent, reason);
		LOG("[RenderScale] %s", g_status);
		return;
	}

	int width = 0;
	int height = 0;
	SizeForPercent(percent, width, height);

	if (!WriteAll(width, height))
	{
		SetStatus("could not write the immediates");
		LOG("[RenderScale] %s", g_status);
		return;
	}

	g_applied = percent != 100;
	g_appliedPercent = percent;

	SetStatus("%dx%d requested (%d%%). The game builds its render targets once, so this takes "
		"effect the next time it does - change a video option in the game's own menu, or restart.",
		width, height, percent);
	LOG("[RenderScale] %s", g_status);
}

void RenderScale::Restore()
{
	if (!g_installed)
		return;

	for (int i = 0; i < g_widthCount; ++i)
		WriteCodeDword(g_widthSites[i].address, g_widthSites[i].original);

	for (int i = 0; i < g_heightCount; ++i)
		WriteCodeDword(g_heightSites[i].address, g_heightSites[i].original);

	g_applied = false;
	g_appliedPercent = 100;
}

bool RenderScale::IsInstalled()
{
	return g_installed;
}

bool RenderScale::IsApplied()
{
	return g_applied;
}

int RenderScale::GetPercent()
{
	return g_appliedPercent;
}

bool RenderScale::GetRequestedSize(int& outWidth, int& outHeight)
{
	SizeForPercent(g_modVals.internalResolutionPercent, outWidth, outHeight);
	return g_installed;
}

// Read once every few frames and cached. The hitbox overlay and the render target test ask this per
// draw, and two SEH guarded reads on a per draw path is not free.
bool RenderScale::GetInForceSize(int& outWidth, int& outHeight)
{
	static int cachedWidth = kBaseWidth;
	static int cachedHeight = kBaseHeight;
	static bool cachedValid = false;
	static DWORD cachedTick = 0;

	const DWORD now = GetTickCount();

	if (cachedTick == 0 || now - cachedTick > 200)
	{
		cachedTick = now;

		uint32_t width = 0;
		uint32_t height = 0;

		if (ReadDword(RvaToAddress(GameOffsets::kRenderTargetWidth), width) &&
			ReadDword(RvaToAddress(GameOffsets::kRenderTargetHeight), height) &&
			width != 0 && height != 0)
		{
			cachedWidth = static_cast<int>(width);
			cachedHeight = static_cast<int>(height);
			cachedValid = true;
		}
		else
		{
			cachedValid = false;
		}
	}

	outWidth = cachedWidth;
	outHeight = cachedHeight;
	return cachedValid;
}

bool RenderScale::GetObservedSize(int& outWidth, int& outHeight, int& outCount)
{
	outWidth = 0;
	outHeight = 0;
	outCount = 0;

	int best = -1;

	for (int i = 0; i < g_observedUsed; ++i)
	{
		const unsigned area = g_observed[i].width * g_observed[i].height;

		if (best < 0 || area > g_observed[best].width * g_observed[best].height)
			best = i;
	}

	if (best < 0)
		return false;

	outWidth = static_cast<int>(g_observed[best].width);
	outHeight = static_cast<int>(g_observed[best].height);
	outCount = g_observed[best].count;
	return true;
}

void RenderScale::NoteCreatedTarget(unsigned width, unsigned height, bool succeeded)
{
	if (!succeeded)
	{
		++g_observedFailures;
		LOG("[RenderScale] a %ux%u render target failed to create", width, height);
		return;
	}

	for (int i = 0; i < g_observedUsed; ++i)
	{
		if (g_observed[i].width == width && g_observed[i].height == height)
		{
			++g_observed[i].count;
			return;
		}
	}

	if (g_observedUsed >= kObservedSlots)
		return;

	g_observed[g_observedUsed].width = width;
	g_observed[g_observedUsed].height = height;
	g_observed[g_observedUsed].count = 1;
	++g_observedUsed;
}

const char* RenderScale::GetStatusText()
{
	return g_status;
}
