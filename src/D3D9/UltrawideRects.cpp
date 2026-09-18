#include "D3D9/UltrawideRects.h"

#include "D3D9/DrawQueue.h"
#include "D3D9/UltrawideHud.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kPairLength = 10;
constexpr int kCallWindow = 24;
constexpr int kMinVotes = 20;
constexpr int kMaxCandidates = 64;
constexpr int kMaxSites = 96;

constexpr uint32_t kAuthoredWidth = 1280;

const uint8_t kPushPair[kPairLength] = { 0x68, 0xd0, 0x02, 0x00, 0x00, 0x68, 0x00, 0x05, 0x00, 0x00 };

const char kFillPrologue[] = "\x55\x8b\xec\x56\x85\xc9\xbe\x00\x00\x00\x00\x57\x0f\x45\xf1\x8b";
const char kFillMask[] = "xxxxxxx????xxxxx";
const char kPastePrologue[] = "\x55\x8b\xec\x83\xe4\xf8\x81\xec\x8c\x00\x00\x00\xa1";
const char kPasteMask[] = "xxxxxxxxxxxxx";

struct Candidate
{
	uintptr_t target;
	int votes;
};

struct Site
{
	uintptr_t returnAddress;
	long hits;
};

void* g_fillOriginal = nullptr;
void* g_pasteOriginal = nullptr;
const uint32_t* g_virtualWidth = nullptr;

uintptr_t g_fill = 0;
uintptr_t g_paste = 0;
bool g_ready = false;
bool g_wanted = false;
bool g_tried = false;
bool g_hooked = false;
bool g_enabled = false;

Site g_sites[kMaxSites] = {};
volatile long g_siteCount = 0;
volatile long g_overflow = 0;

char g_status[160] = "off";

void __stdcall NoteSite(uintptr_t returnAddress)
{
	const long count = g_siteCount < kMaxSites ? g_siteCount : kMaxSites;

	for (long i = 0; i < count; ++i)
	{
		if (g_sites[i].returnAddress != returnAddress)
			continue;

		InterlockedIncrement(&g_sites[i].hits);
		return;
	}

	const long slot = InterlockedIncrement(&g_siteCount) - 1;
	if (slot >= kMaxSites)
	{
		InterlockedIncrement(&g_overflow);
		return;
	}

	g_sites[slot].returnAddress = returnAddress;
	g_sites[slot].hits = 1;
}

constexpr uint32_t kAuthoredHeight = 720;
constexpr uint32_t kLargePaste = 640;
constexpr long kProbedPastes = 48;

volatile long g_probeLeft = 0;

enum PasteSlot
{
	Slot_Return,
	Slot_DstX,
	Slot_DstY,
	Slot_DstW,
	Slot_DstH,
	Slot_SrcX,
	Slot_SrcY,
	Slot_SrcW,
	Slot_SrcH,
};

bool ReadTextureSize(uint32_t texture, int32_t outSize[2])
{
	uint32_t record = 0;

	return texture != 0 &&
		TryReadMemory(&record, reinterpret_cast<const void*>(texture), sizeof(record)) &&
		record != 0 &&
		TryReadMemory(outSize, reinterpret_cast<const void*>(record + 0x2c), sizeof(int32_t) * 2);
}

void Probe(const uint32_t* stack, uint32_t texture)
{
	if ((stack[Slot_DstW] < kLargePaste && stack[Slot_SrcW] < kLargePaste) ||
		InterlockedDecrement(&g_probeLeft) < 0)
	{
		return;
	}

	int32_t size[2] = {};
	const bool known = ReadTextureSize(texture, size);

	LOG_RAW("paste from rva 0x%06x  dst %d,%d %dx%d  src %d,%d %dx%d  texture %08x %dx%d%s",
		static_cast<unsigned>(stack[Slot_Return] - GetGameBaseAddress()), stack[Slot_DstX],
		stack[Slot_DstY], stack[Slot_DstW], stack[Slot_DstH], stack[Slot_SrcX], stack[Slot_SrcY],
		stack[Slot_SrcW], stack[Slot_SrcH], texture, size[0], size[1], known ? "" : " (unread)");
}

bool WidenDestination(uint32_t* stack, uint32_t width)
{
	if (stack[Slot_DstX] != 0 || stack[Slot_DstW] != kAuthoredWidth)
		return false;

	stack[Slot_DstW] = width;
	return true;
}

bool WidenSource(uint32_t* stack, uint32_t texture)
{
	if (stack[Slot_SrcX] != 0 || stack[Slot_SrcY] != 0 || stack[Slot_SrcW] != kAuthoredWidth ||
		stack[Slot_SrcH] != kAuthoredHeight)
	{
		return false;
	}

	int32_t size[2] = {};
	if (!ReadTextureSize(texture, size) || size[0] <= static_cast<int32_t>(kAuthoredWidth) ||
		size[1] != static_cast<int32_t>(kAuthoredHeight))
	{
		return false;
	}

	stack[Slot_SrcW] = static_cast<uint32_t>(size[0]);
	return true;
}

enum FillSlot
{
	Fill_Return,
	Fill_Y,
	Fill_W,
};

enum SavedRegister
{
	Saved_Edi,
	Saved_Esi,
	Saved_Ebp,
	Saved_Esp,
	Saved_Ebx,
	Saved_Edx,
	Saved_Ecx,
	Saved_Eax,
	Saved_Count,
};

void __stdcall AdjustPaste(uint32_t* stack, uint32_t texture)
{
	if (g_probeLeft > 0)
		Probe(stack, texture);

	const uint32_t width = *g_virtualWidth;
	if (width <= kAuthoredWidth)
		return;

	if (UltrawideHud::IsDrawing())
		return;

	const bool destination = WidenDestination(stack, width);
	const bool source = WidenSource(stack, texture);

	if (destination || source)
		NoteSite(stack[Slot_Return]);
}

void __stdcall AdjustFill(uint32_t* saved)
{
	const uint32_t width = *g_virtualWidth;
	if (width <= kAuthoredWidth)
		return;

	if (UltrawideHud::IsDrawing())
		return;

	uint32_t* const stack = saved + Saved_Count;
	if (saved[Saved_Edx] != 0 || stack[Fill_W] != kAuthoredWidth)
		return;

	stack[Fill_W] = width;
	NoteSite(stack[Fill_Return]);
}

__declspec(naked) void PasteDetour()
{
	__asm
	{
		pushad
		lea eax, [esp + 0x20]
		push edx
		push eax
		call AdjustPaste
		popad
		jmp dword ptr [g_pasteOriginal]
	}
}

__declspec(naked) void FillDetour()
{
	__asm
	{
		pushad
		push esp
		call AdjustFill
		popad
		jmp dword ptr [g_fillOriginal]
	}
}

bool InText(uintptr_t address, uintptr_t start, size_t size)
{
	return address >= start && address < start + size;
}

void Vote(Candidate* candidates, int& count, uintptr_t target)
{
	for (int i = 0; i < count; ++i)
	{
		if (candidates[i].target != target)
			continue;

		++candidates[i].votes;
		return;
	}

	if (count >= kMaxCandidates)
		return;

	candidates[count++] = { target, 1 };
}

void CollectCallTargets(uintptr_t start, size_t size, Candidate* candidates, int& count)
{
	const auto* text = reinterpret_cast<const uint8_t*>(start);

	for (size_t i = 0; i + kPairLength + kCallWindow + 5 <= size; ++i)
	{
		if (memcmp(text + i, kPushPair, kPairLength) != 0)
			continue;

		for (size_t j = i + kPairLength; j < i + kPairLength + kCallWindow; ++j)
		{
			if (text[j] != 0xe8)
				continue;

			int32_t relative = 0;
			memcpy(&relative, text + j + 1, sizeof(relative));

			const uintptr_t target = start + j + 5 + relative;
			if (!InText(target, start, size))
				continue;

			Vote(candidates, count, target);
			break;
		}
	}
}

bool Matches(uintptr_t target, const char* prologue, const char* mask)
{
	return HookManager::FindPatternInRange(target, strlen(mask), prologue, mask) == target;
}

bool Locate()
{
	uintptr_t start = 0;
	size_t size = 0;

	if (!HookManager::GetSectionBounds(".text", start, size))
		return false;

	Candidate candidates[kMaxCandidates] = {};
	int count = 0;
	CollectCallTargets(start, size, candidates, count);

	for (int i = 0; i < count; ++i)
	{
		const Candidate& candidate = candidates[i];
		if (candidate.votes < kMinVotes)
			continue;

		if (g_fill == 0 && Matches(candidate.target, kFillPrologue, kFillMask))
			g_fill = candidate.target;
		else if (g_paste == 0 && Matches(candidate.target, kPastePrologue, kPasteMask))
			g_paste = candidate.target;
	}

	return g_fill != 0 && g_paste != 0;
}

bool Install()
{
	if (g_hooked)
		return true;

	if (g_tried)
		return false;

	g_tried = true;
	g_virtualWidth = reinterpret_cast<const uint32_t*>(
		RvaToAddress(GameOffsets::kRenderVirtualWidth));

	if (g_virtualWidth == nullptr || !Locate())
	{
		snprintf(g_status, sizeof(g_status), "the two 2D draw calls were not found, full-screen "
			"layers stay 1280 wide");
		LOG("[UltrawideRects] %s", g_status);
		return false;
	}

	if (!HookManager::CreateHook(reinterpret_cast<void*>(g_fill),
			reinterpret_cast<void*>(&FillDetour), &g_fillOriginal, "Ultrawide fill") ||
		!HookManager::CreateHook(reinterpret_cast<void*>(g_paste),
			reinterpret_cast<void*>(&PasteDetour), &g_pasteOriginal, "Ultrawide paste"))
	{
		snprintf(g_status, sizeof(g_status), "the 2D draw hooks could not be created");
		LOG("[UltrawideRects] %s", g_status);
		return false;
	}

	g_hooked = true;
	UltrawideHud::Install();
	DrawQueue::Install();
	LOG("[UltrawideRects] fill at rva 0x%x, paste at rva 0x%x",
		static_cast<unsigned>(g_fill - GetGameBaseAddress()),
		static_cast<unsigned>(g_paste - GetGameBaseAddress()));
	return true;
}

void SetEnabled(bool enabled)
{
	if (g_enabled == enabled)
		return;

	const bool fill = HookManager::SetHookEnabled(reinterpret_cast<void*>(g_fill), enabled);
	const bool paste = HookManager::SetHookEnabled(reinterpret_cast<void*>(g_paste), enabled);
	UltrawideHud::SetEnabled(enabled);
	DrawQueue::SetEnabled(enabled);
	g_enabled = enabled && fill && paste;
}

}

void UltrawideRects::Initialize()
{
	g_ready = true;
	Apply(g_wanted);
}

void UltrawideRects::Apply(bool widened)
{
	g_wanted = widened;

	if (!g_ready)
		return;

	if (!widened)
	{
		if (g_hooked)
			SetEnabled(false);

		snprintf(g_status, sizeof(g_status), "off");
		return;
	}

	if (!Install())
		return;

	SetEnabled(true);
	snprintf(g_status, sizeof(g_status), g_enabled
		? "full-screen 2D layers follow the widened width"
		: "the 2D draw hooks would not enable");
	LOG("[UltrawideRects] %s", g_status);
}

void UltrawideRects::LogSites()
{
	InterlockedExchange(&g_probeLeft, kProbedPastes);

	const long count = g_siteCount < kMaxSites ? g_siteCount : kMaxSites;
	const uintptr_t base = GetGameBaseAddress();

	LOG_RAW("UltrawideRects: %s, %ld call sites widened so far%s", g_status, count,
		g_overflow > 0 ? " (table full)" : "");

	for (long i = 0; i < count; ++i)
	{
		LOG_RAW("  returns to rva 0x%06x  %ld hits",
			static_cast<unsigned>(g_sites[i].returnAddress - base), g_sites[i].hits);
	}
}

const char* UltrawideRects::GetStatusText()
{
	return g_status;
}
