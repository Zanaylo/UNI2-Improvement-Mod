#include "Core/Input/WindowsPointer.h"

#include "Core/Config/interfaces.h"
#include "Core/Input/PointerRule.h"
#include "Core/logger.h"
#include "Hooks/ImportPatch.h"

#include <Windows.h>

namespace {

PointerRule::ShowCursor_t g_original = nullptr;
bool g_restored = false;

int WINAPI KeepShown(BOOL show)
{
	return PointerRule::Answer(show, g_original);
}

}

bool WindowsPointer::Install()
{
	if (!g_modVals.keepWindowsPointer || g_original != nullptr)
		return false;

	void* previous = nullptr;

	if (!ImportPatch::Replace(GetModuleHandleA(nullptr), "USER32.dll", "ShowCursor", reinterpret_cast<void*>(&KeepShown),
		&previous))
	{
		LOG("WindowsPointer: the game's ShowCursor import was not found, the game may still hide the pointer");
		return false;
	}

	g_original = reinterpret_cast<PointerRule::ShowCursor_t>(previous);
	LOG("WindowsPointer: the game can no longer hide the Windows pointer");
	return true;
}

void WindowsPointer::OnWindowMessage()
{
	if (g_original == nullptr || g_restored)
		return;

	g_restored = true;

	if (!PointerRule::Restore(g_original))
		LOG("WindowsPointer: the pointer the game hid before the mod loaded could not be shown again");
}

bool WindowsPointer::IsKept()
{
	return g_original != nullptr;
}
