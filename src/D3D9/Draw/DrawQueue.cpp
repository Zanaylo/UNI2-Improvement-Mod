#include "D3D9/Draw/DrawQueue.h"

#include "Core/logger.h"
#include "D3D9/Draw/DrawQueueCensus.h"
#include "D3D9/Draw/DrawQueueProbe.h"
#include "D3D9/Draw/LayerFadeProbe.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdint>

namespace {

const char kPushPrologue[] =
	"\x55\x8b\xec\x53\x57\x8b\xf9\x8b\x1f\x85\xdb\x74\x63\x56\x8b\x75";
const char kPushMask[] = "xxxxxxxxxxxxxxxx";

void* g_original = nullptr;
uintptr_t g_target = 0;
bool g_tried = false;
bool g_enabled = false;
constexpr int kMostObservers = 4;

uint32_t g_users = 0;
DrawQueue::IObserver* g_observers[kMostObservers] = {};
int g_observerCount = 0;

bool Links(uint32_t queue, const void* command)
{
	return *reinterpret_cast<const uint32_t*>(queue) != 0 && *static_cast<const uint32_t*>(command) == 0;
}

uint32_t Observed(uint32_t layer, void* command)
{
	for (int i = 0; i < g_observerCount; ++i)
		layer = g_observers[i]->OnPush(layer, command);

	return layer;
}

uint32_t __stdcall OnPush(uint32_t layer, const uint32_t* stack, const uint32_t* frame, uint32_t queue)
{
	void* const command = reinterpret_cast<void*>(stack[1]);

	DrawQueueProbe::Note(queue, layer, command, stack[0], frame);
	DrawQueueCensus::Note(layer, command, stack[0], frame);
	LayerFadeProbe::Note(layer, command);

	return Links(queue, command) ? Observed(layer, command) : layer;
}

__declspec(naked) void PushDetour()
{
	__asm
	{
		pushad
		lea eax, [esp + 0x20]
		push ecx
		push ebp
		push eax
		push edx
		call OnPush
		mov [esp + 0x14], eax
		popad
		jmp dword ptr [g_original]
	}
}

}

bool DrawQueue::Install()
{
	if (g_target != 0)
		return true;

	if (g_tried)
		return false;

	g_tried = true;

	uintptr_t start = 0;
	size_t size = 0;
	if (!HookManager::GetSectionBounds(".text", start, size))
		return false;

	const uintptr_t target = HookManager::FindPatternInRange(start, size, kPushPrologue, kPushMask);
	if (target == 0 || !HookManager::CreateHook(reinterpret_cast<void*>(target),
		reinterpret_cast<void*>(&PushDetour), &g_original, "Draw queue"))
	{
		LOG("[DrawQueue] the queue push was not found");
		return false;
	}

	g_target = target;
	return true;
}

void DrawQueue::Want(User user, bool wanted)
{
	g_users = wanted ? g_users | user : g_users & ~static_cast<uint32_t>(user);

	const bool enabled = g_users != 0;

	if (g_target == 0 || g_enabled == enabled)
		return;

	g_enabled = HookManager::SetHookEnabled(reinterpret_cast<void*>(g_target), enabled) && enabled;
}

bool DrawQueue::AddObserver(IObserver* observer)
{
	if (observer == nullptr || g_observerCount >= kMostObservers)
		return false;

	g_observers[g_observerCount++] = observer;
	return true;
}
