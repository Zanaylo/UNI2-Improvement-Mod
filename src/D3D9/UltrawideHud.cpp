#include "D3D9/UltrawideHud.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/QueuedQuad.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>

namespace {

constexpr int kClassCount = 7;
constexpr int kSlotCount = 4;
constexpr int kElementCount = 20;
constexpr long kMaxDepth = 8;
constexpr uint32_t kAuthoredWidth = 1280;

const char* const kClasses[kClassCount] = {
	".?AVIBCockpit_Interface@@",
	".?AVCBCockpit_HP@@",
	".?AVCBCockpit_Info@@",
	".?AVCBCockpit_WinCount@@",
	".?AVCBCockpit_CharaGrp@@",
	".?AVCBCockpit_Reactor@@",
	".?AVCBCockpit_PlayerInfo@@",
};

const int kDrawSlots[kSlotCount] = { 2, 3, 4, 5 };

struct Signature
{
	const char* label;
	const char* pattern;
	const char* mask;
};

const Signature kDrawers[] = {
	{
		"GRD gauge",
		"\x53\x8b\xdc\x83\xec\x08\x83\xe4\xf8\x83\xc4\x04\x55\x8b\x6b\x04\x89\x6c\x24\x04"
		"\x8b\xec\x8b\x15\x00\x00\x00\x00\x83\xec\x18\x56\x57\x8b\x3d\x00\x00\x00\x00\x33\xf6",
		"xxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxx????xx",
	},
	{
		"UI panel",
		"\x55\x8b\xec\x83\xe4\xf8\x83\xec\x54\xa1\x00\x00\x00\x00\x33\xc4\x89\x44\x24\x50"
		"\x53\x56\x57\x8b\xf9\x83\xbf\x98",
		"xxxxxxxxxx????xxxxxxxxxxxxxx",
	},
	{
		"training info text",
		"\x55\x8b\xec\x6a\xff\x68\x00\x00\x00\x00\x64\xa1\x00\x00\x00\x00\x50\x83\xec\x34"
		"\xa1\x00\x00\x00\x00\x33\xc5\x89\x45\xf0\x53\x56\x57\x50\x8d\x45\xf4\x64\xa3\x00"
		"\x00\x00\x00\x8b\xd9\xc7\x45\xe8",
		"xxxxxx????xxxxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxx",
	},
	{
		"training info values",
		"\x55\x8b\xec\x6a\xff\x68\x00\x00\x00\x00\x64\xa1\x00\x00\x00\x00\x50\x83\xec\x3c"
		"\xa1\x00\x00\x00\x00\x33\xc5\x89\x45\xf0\x53\x56\x57\x50\x8d\x45\xf4\x64\xa3\x00"
		"\x00\x00\x00\x8b\xf1\xba\x0f\x00",
		"xxxxxx????xxxxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxx",
	},
};

void* g_originals[kElementCount] = {};
void* g_targets[kElementCount] = {};
uint32_t g_returns[kMaxDepth] = {};
long g_depth = 0;
uint32_t g_renderThread = 0;

bool g_tried = false;
int g_hooked = 0;
bool g_enabled = false;

char g_status[128] = "off";

__declspec(naked) void HudExit()
{
	__asm
	{
		sub esp, 4
		push eax
		mov eax, g_depth
		dec eax
		mov g_depth, eax
		mov eax, dword ptr [g_returns + eax * 4]
		mov dword ptr [esp + 4], eax
		pop eax
		ret
	}
}

#define ULTRAWIDE_HUD_ENTER(index)                                   \
	__declspec(naked) void Enter##index()                            \
	{                                                                \
		__asm mov eax, fs:[0x24]                                     \
		__asm cmp eax, g_renderThread                                \
		__asm jne pass                                               \
		__asm mov eax, g_depth                                       \
		__asm cmp eax, 8                                             \
		__asm jae pass                                               \
		__asm mov edx, dword ptr [esp]                               \
		__asm mov dword ptr [g_returns + eax * 4], edx               \
		__asm inc eax                                                \
		__asm mov g_depth, eax                                       \
		__asm mov dword ptr [esp], offset HudExit                     \
		__asm pass:                                                  \
		__asm jmp dword ptr [g_originals + index * 4]                \
	}

ULTRAWIDE_HUD_ENTER(0)
ULTRAWIDE_HUD_ENTER(1)
ULTRAWIDE_HUD_ENTER(2)
ULTRAWIDE_HUD_ENTER(3)
ULTRAWIDE_HUD_ENTER(4)
ULTRAWIDE_HUD_ENTER(5)
ULTRAWIDE_HUD_ENTER(6)
ULTRAWIDE_HUD_ENTER(7)
ULTRAWIDE_HUD_ENTER(8)
ULTRAWIDE_HUD_ENTER(9)
ULTRAWIDE_HUD_ENTER(10)
ULTRAWIDE_HUD_ENTER(11)
ULTRAWIDE_HUD_ENTER(12)
ULTRAWIDE_HUD_ENTER(13)
ULTRAWIDE_HUD_ENTER(14)
ULTRAWIDE_HUD_ENTER(15)
ULTRAWIDE_HUD_ENTER(16)
ULTRAWIDE_HUD_ENTER(17)
ULTRAWIDE_HUD_ENTER(18)
ULTRAWIDE_HUD_ENTER(19)

#undef ULTRAWIDE_HUD_ENTER

void* const kEnters[kElementCount] = {
	reinterpret_cast<void*>(&Enter0), reinterpret_cast<void*>(&Enter1),
	reinterpret_cast<void*>(&Enter2), reinterpret_cast<void*>(&Enter3),
	reinterpret_cast<void*>(&Enter4), reinterpret_cast<void*>(&Enter5),
	reinterpret_cast<void*>(&Enter6), reinterpret_cast<void*>(&Enter7),
	reinterpret_cast<void*>(&Enter8), reinterpret_cast<void*>(&Enter9),
	reinterpret_cast<void*>(&Enter10), reinterpret_cast<void*>(&Enter11),
	reinterpret_cast<void*>(&Enter12), reinterpret_cast<void*>(&Enter13),
	reinterpret_cast<void*>(&Enter14), reinterpret_cast<void*>(&Enter15),
	reinterpret_cast<void*>(&Enter16), reinterpret_cast<void*>(&Enter17),
	reinterpret_cast<void*>(&Enter18), reinterpret_cast<void*>(&Enter19),
};

bool Known(void* draw)
{
	for (int i = 0; i < g_hooked; ++i)
	{
		if (g_targets[i] == draw)
			return true;
	}

	return false;
}

void Hook(void* draw, const char* label)
{
	if (draw == nullptr || Known(draw) || g_hooked >= kElementCount)
		return;

	if (!HookManager::CreateHook(draw, kEnters[g_hooked], &g_originals[g_hooked], label))
		return;

	g_targets[g_hooked++] = draw;
}

void HookClass(const char* mangledName)
{
	const uintptr_t vtable = HookManager::FindRttiVTable(mangledName);
	if (vtable == 0)
		return;

	for (const int slot : kDrawSlots)
		Hook(reinterpret_cast<void* const*>(vtable)[slot], mangledName);
}

void HookDrawers()
{
	uintptr_t start = 0;
	size_t size = 0;
	if (!HookManager::GetSectionBounds(".text", start, size))
		return;

	for (const Signature& drawer : kDrawers)
	{
		const uintptr_t draw = HookManager::FindPatternInRange(start, size, drawer.pattern,
			drawer.mask);

		if (draw == 0)
		{
			LOG("[UltrawideHud] the %s draw was not found", drawer.label);
			continue;
		}

		Hook(reinterpret_cast<void*>(draw), drawer.label);
	}
}

}

bool UltrawideHud::Install()
{
	if (g_tried)
		return g_hooked > 0;

	g_tried = true;

	for (const char* const mangledName : kClasses)
		HookClass(mangledName);

	HookDrawers();

	snprintf(g_status, sizeof(g_status), "%d HUD draw functions hooked", g_hooked);
	LOG("[UltrawideHud] %s", g_status);
	return g_hooked > 0;
}

void UltrawideHud::SetEnabled(bool enabled)
{
	if (g_enabled == enabled)
		return;

	for (int i = 0; i < kElementCount; ++i)
	{
		if (g_targets[i] != nullptr)
			HookManager::SetHookEnabled(g_targets[i], enabled);
	}

	g_enabled = enabled;
}

void UltrawideHud::SetRenderThread(uint32_t threadId)
{
	g_renderThread = threadId;
}

bool UltrawideHud::IsDrawing()
{
	if (g_depth <= 0 || GetCurrentThreadId() != g_renderThread)
		return false;

	static const uint32_t* const cockpit = reinterpret_cast<const uint32_t*>(
		RvaToAddress(GameOffsets::kBattleCockpit));

	return cockpit != nullptr && *cockpit != 0;
}

void UltrawideHud::ShiftCommand(void* command)
{
	if (command == nullptr || !IsDrawing())
		return;

	static const uint32_t* const virtualWidth = reinterpret_cast<const uint32_t*>(
		RvaToAddress(GameOffsets::kRenderVirtualWidth));

	if (virtualWidth == nullptr || *virtualWidth <= kAuthoredWidth)
		return;

	auto* const quad = static_cast<QueuedQuad*>(command);
	if (quad->type != kQueuedQuadType)
		return;

	const float shift = static_cast<float>(*virtualWidth - kAuthoredWidth) * 0.5f;

	for (QueuedCorner& corner : quad->corners)
		corner.x += shift;
}

const char* UltrawideHud::GetStatusText()
{
	return g_status;
}
