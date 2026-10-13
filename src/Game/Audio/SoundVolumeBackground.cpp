#include "Game/Audio/SoundVolumeBackground.h"

#include "Core/Config/interfaces.h"
#include "Core/Profiler.h"
#include "Core/SpikeWatch.h"
#include "Core/logger.h"
#include "Game/Audio/VolumeQueue.h"
#include "Hooks/GameHook.h"

#include <Windows.h>
#include <mmreg.h>
#include <dsound.h>

#include <cstdio>

namespace {

constexpr int kReleaseIndex = 2;
constexpr int kSetVolumeIndex = 15;
constexpr DWORD kProbeBytes = 4096;
constexpr WORD kProbeChannels = 2;
constexpr DWORD kProbeRate = 44100;
constexpr WORD kProbeBits = 16;
constexpr LONG kBurstCalls = 32;

using DirectSoundCreate8_t = HRESULT(WINAPI*)(LPCGUID, LPDIRECTSOUND8*, LPUNKNOWN);
using SetVolume_t = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, LONG);
using Release_t = ULONG(STDMETHODCALLTYPE*)(IDirectSoundBuffer*);

GameHook<SetVolume_t> g_setVolumeHook("IDirectSoundBuffer::SetVolume");
GameHook<Release_t> g_releaseHook("IDirectSoundBuffer::Release");

SRWLOCK g_queueLock = SRWLOCK_INIT;
VolumeQueue g_queue;
HANDLE g_wake = nullptr;
volatile LONG g_waiting = 0;
void* g_inFlight = nullptr;
volatile LONG g_callsThisFrame = 0;

volatile LONG g_calls = 0;
volatile LONG g_deferred = 0;
volatile LONG64 g_gameTicks = 0;
volatile LONG64 g_deferTicks = 0;
volatile LONG g_applied = 0;
volatile LONG64 g_backgroundTicks = 0;

LONG g_seenCalls = 0;
LONG g_seenDeferred = 0;
LONG64 g_seenGameTicks = 0;
LONG64 g_seenDeferTicks = 0;

char g_status[192] = "not installed";

bool TryPush(const VolumeQueue::Change& change)
{
	AcquireSRWLockExclusive(&g_queueLock);
	const bool pushed = g_queue.Push(change);
	ReleaseSRWLockExclusive(&g_queueLock);

	return pushed;
}

bool TryPop(VolumeQueue::Change& out)
{
	AcquireSRWLockExclusive(&g_queueLock);
	const bool popped = g_queue.Pop(out);
	g_inFlight = popped ? out.buffer : nullptr;
	ReleaseSRWLockExclusive(&g_queueLock);

	return popped;
}

void Landed()
{
	AcquireSRWLockExclusive(&g_queueLock);
	g_inFlight = nullptr;
	ReleaseSRWLockExclusive(&g_queueLock);
}

bool StillWaiting(const void* buffer)
{
	AcquireSRWLockShared(&g_queueLock);
	const bool waiting = g_inFlight == buffer || g_queue.Holds(buffer);
	ReleaseSRWLockShared(&g_queueLock);

	return waiting;
}

void Defer(IDirectSoundBuffer* buffer, LONG volume)
{
	const int64_t began = Profiler::Now();
	InterlockedIncrement(&g_waiting);

	while (!TryPush({ buffer, volume }))
		SwitchToThread();

	InterlockedIncrement(&g_deferred);
	SetEvent(g_wake);
	InterlockedAdd64(&g_deferTicks, Profiler::Now() - began);
}

void ApplyWaiting()
{
	VolumeQueue::Change change = {};

	while (TryPop(change))
	{
		IDirectSoundBuffer* const buffer = static_cast<IDirectSoundBuffer*>(change.buffer);
		const int64_t began = Profiler::Now();

		g_setVolumeHook.Original()(buffer, change.volume);
		Landed();

		InterlockedAdd64(&g_backgroundTicks, Profiler::Now() - began);
		InterlockedIncrement(&g_applied);
		InterlockedDecrement(&g_waiting);
	}
}

DWORD WINAPI Worker(LPVOID)
{
	while (WaitForSingleObject(g_wake, INFINITE) == WAIT_OBJECT_0)
		ApplyWaiting();

	return 0;
}

ULONG STDMETHODCALLTYPE HookedRelease(IDirectSoundBuffer* buffer)
{
	while (InterlockedCompareExchange(&g_waiting, 0, 0) > 0 && StillWaiting(buffer))
		SwitchToThread();

	return g_releaseHook.Original()(buffer);
}

HRESULT STDMETHODCALLTYPE HookedSetVolume(IDirectSoundBuffer* buffer, LONG volume)
{
	InterlockedIncrement(&g_calls);
	const int callsBefore = InterlockedIncrement(&g_callsThisFrame) - 1;

	if (g_modVals.soundVolumeBackground && VolumeQueue::ShouldDefer(callsBefore, g_waiting))
	{
		Defer(buffer, volume);
		return DS_OK;
	}

	const int64_t began = Profiler::Now();
	const HRESULT result = g_setVolumeHook.Original()(buffer, volume);
	const int64_t spent = Profiler::Now() - began;

	InterlockedAdd64(&g_gameTicks, spent);
	SpikeWatch::Add(SpikeReport::Cost_SoundVolume, spent);

	return result;
}

WAVEFORMATEX ProbeFormat()
{
	WAVEFORMATEX format = {};
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = kProbeChannels;
	format.nSamplesPerSec = kProbeRate;
	format.wBitsPerSample = kProbeBits;
	format.nBlockAlign = static_cast<WORD>(kProbeChannels * kProbeBits / 8);
	format.nAvgBytesPerSec = kProbeRate * format.nBlockAlign;

	return format;
}

IDirectSoundBuffer* CreateProbeBuffer(IDirectSound8* sound)
{
	WAVEFORMATEX format = ProbeFormat();

	DSBUFFERDESC description = {};
	description.dwSize = sizeof(description);
	description.dwFlags = DSBCAPS_CTRLVOLUME;
	description.dwBufferBytes = kProbeBytes;
	description.lpwfxFormat = &format;

	IDirectSoundBuffer* buffer = nullptr;

	if (FAILED(sound->CreateSoundBuffer(&description, &buffer, nullptr)))
		return nullptr;

	return buffer;
}

void** BufferVtable()
{
	const HMODULE module = GetModuleHandleA("dsound.dll");
	const DirectSoundCreate8_t create = module == nullptr
		? nullptr : reinterpret_cast<DirectSoundCreate8_t>(GetProcAddress(module, "DirectSoundCreate8"));

	IDirectSound8* sound = nullptr;

	if (create == nullptr || FAILED(create(nullptr, &sound, nullptr)) || sound == nullptr)
		return nullptr;

	sound->SetCooperativeLevel(GetDesktopWindow(), DSSCL_NORMAL);

	IDirectSoundBuffer* const buffer = CreateProbeBuffer(sound);
	void** const vtable = buffer != nullptr ? *reinterpret_cast<void***>(buffer) : nullptr;

	if (buffer != nullptr)
		buffer->Release();

	sound->Release();
	return vtable;
}

bool StartWorker()
{
	g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);

	if (g_wake == nullptr)
		return false;

	const HANDLE thread = CreateThread(nullptr, 0, &Worker, nullptr, 0, nullptr);

	if (thread == nullptr)
		return false;

	CloseHandle(thread);
	return true;
}

bool Refuse(const char* why)
{
	strncpy_s(g_status, why, _TRUNCATE);
	LOG("SoundVolumeBackground: %s", g_status);
	return false;
}

}

bool SoundVolumeBackground::Install()
{
	if (!StartWorker())
		return Refuse("no background thread, volumes are set the game's way");

	void** const vtable = BufferVtable();

	if (vtable == nullptr)
		return Refuse("no sound device to find the volume call on, volumes are set the game's way");

	if (!g_releaseHook.Install(vtable[kReleaseIndex], &HookedRelease))
		return Refuse("the sound release could not be hooked, volumes are set the game's way");

	if (!g_setVolumeHook.Install(vtable[kSetVolumeIndex], &HookedSetVolume))
		return Refuse("the volume call could not be hooked, volumes are set the game's way");

	strncpy_s(g_status, "on, no burst of volume changes seen yet", _TRUNCATE);
	LOG("SoundVolumeBackground: hooked IDirectSoundBuffer::SetVolume");
	return true;
}

void SoundVolumeBackground::OnFrame()
{
	const LONG callsNow = g_calls;
	const LONG deferredNow = g_deferred;
	const LONG64 gameTicksNow = g_gameTicks;
	const LONG64 deferTicksNow = g_deferTicks;

	const LONG calls = callsNow - g_seenCalls;
	const LONG deferred = deferredNow - g_seenDeferred;
	const double gameMs = Profiler::ToMs(gameTicksNow - g_seenGameTicks);
	const double deferMs = Profiler::ToMs(deferTicksNow - g_seenDeferTicks);

	g_seenCalls = callsNow;
	g_seenDeferred = deferredNow;
	g_seenGameTicks = gameTicksNow;
	g_seenDeferTicks = deferTicksNow;
	InterlockedExchange(&g_callsThisFrame, 0);

	if (calls < kBurstCalls)
		return;

	sprintf_s(g_status, "last burst: %ld volume changes, %ld set in the background, %.1f ms left on the game",
		calls, deferred, gameMs);
	LOG("SoundVolumeBackground: %ld volume changes in one frame, %ld moved to the background, %.1f ms setting "
		"and %.1f ms handing over on the game thread; %ld applied in the background so far in %.0f ms", calls,
		deferred, gameMs, deferMs, static_cast<long>(g_applied), Profiler::ToMs(g_backgroundTicks));
}

const char* SoundVolumeBackground::StatusText()
{
	return g_status;
}
