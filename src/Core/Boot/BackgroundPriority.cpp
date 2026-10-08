#include "Core/Boot/BackgroundPriority.h"

#include "Core/Boot/BackgroundBoost.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"

#include <Windows.h>

namespace {

typedef LONG(APIENTRY* SetGpuPriorityFn)(HANDLE, int);
typedef LONG(APIENTRY* GetGpuPriorityFn)(HANDLE, int*);

constexpr int kGpuPriorityAboveNormal = 3;
constexpr int kGpuPriorityNormal = 2;
constexpr LONG kStatusSuccess = 0;

struct Saved
{
	DWORD processClass;
	int threadPriority;
	int gpuPriority;
	HANDLE thread;
};

BackgroundBoost g_boost;
Saved g_saved = {};

SetGpuPriorityFn ResolveSetGpuPriority()
{
	static const SetGpuPriorityFn resolved = reinterpret_cast<SetGpuPriorityFn>(
		GetProcAddress(GetModuleHandleA("gdi32.dll"), "D3DKMTSetProcessSchedulingPriorityClass"));

	return resolved;
}

GetGpuPriorityFn ResolveGetGpuPriority()
{
	static const GetGpuPriorityFn resolved = reinterpret_cast<GetGpuPriorityFn>(
		GetProcAddress(GetModuleHandleA("gdi32.dll"), "D3DKMTGetProcessSchedulingPriorityClass"));

	return resolved;
}

int ReadGpuPriority()
{
	const GetGpuPriorityFn get = ResolveGetGpuPriority();
	int priority = kGpuPriorityNormal;

	if (get == nullptr || get(GetCurrentProcess(), &priority) != kStatusSuccess)
		return kGpuPriorityNormal;

	return priority;
}

bool WriteGpuPriority(int priority)
{
	const SetGpuPriorityFn set = ResolveSetGpuPriority();

	return set != nullptr && set(GetCurrentProcess(), priority) == kStatusSuccess;
}

bool IsOnline()
{
	const NetLink::Snapshot& link = NetLink::Current();

	return link.lobby != 0 || link.backend != NetLink::Backend_None;
}

void Raise()
{
	g_saved.processClass = GetPriorityClass(GetCurrentProcess());
	g_saved.thread = GetCurrentThread();
	g_saved.threadPriority = GetThreadPriority(g_saved.thread);
	g_saved.gpuPriority = ReadGpuPriority();

	const bool process = SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS) != FALSE;
	const bool thread = SetThreadPriority(g_saved.thread, THREAD_PRIORITY_ABOVE_NORMAL) != FALSE;
	const bool gpu = WriteGpuPriority(kGpuPriorityAboveNormal);

	NetLog::Write("background priority: the game is in the background, raised (process %d, game thread %d, gpu %d)",
		process ? 1 : 0, thread ? 1 : 0, gpu ? 1 : 0);
}

void Restore()
{
	SetPriorityClass(GetCurrentProcess(), g_saved.processClass);
	SetThreadPriority(g_saved.thread, g_saved.threadPriority);
	WriteGpuPriority(g_saved.gpuPriority);

	NetLog::Write("background priority: restored");
}

}

void BackgroundPriority::Update()
{
	switch (g_boost.Update(g_modVals.backgroundFullSpeed, HotkeyFocus(), IsOnline()))
	{
	case BackgroundBoost::Change_Raise:
		Raise();
		return;
	case BackgroundBoost::Change_Restore:
		Restore();
		return;
	default:
		return;
	}
}

bool BackgroundPriority::IsRaised()
{
	return g_boost.IsRaised();
}
