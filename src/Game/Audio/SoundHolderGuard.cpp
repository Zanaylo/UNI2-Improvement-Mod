#include "Game/Audio/SoundHolderGuard.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Audio/SoundHolderCheck.h"
#include "Game/Audio/SoundVolumeBackground.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/GameHook.h"

#include <Windows.h>

namespace {

using StopSoundHolder_t = void(__fastcall*)(void** slot, void* unused);

GameHook<StopSoundHolder_t> g_stopHook("StopSoundHolder");
volatile LONG g_dropped = 0;
volatile LONG g_seen = 0;

uint32_t ReadPointer(uint32_t address)
{
	uint32_t value = 0;

	if (address == 0 || !TryReadDword(reinterpret_cast<const void*>(static_cast<uintptr_t>(address)), value))
		return 0;

	return value;
}

bool BufferAlive(uint32_t buffer)
{
	uint32_t vtable = 0;
	const bool readable = TryReadDword(reinterpret_cast<const void*>(static_cast<uintptr_t>(buffer)), vtable);

	return SoundHolderCheck::BufferAlive(readable, vtable, SoundVolumeBackground::BufferVtable());
}

void DropFreedBuffer(void** slot)
{
	const uint32_t holder = ReadPointer(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(slot)));
	const uint32_t player = ReadPointer(holder + static_cast<uint32_t>(GameOffsets::kSoundHolderPlayer));

	if (player == 0)
		return;

	const uint32_t bufferField = player + static_cast<uint32_t>(GameOffsets::kSoundPlayerBuffer);
	const uint32_t buffer = ReadPointer(bufferField);

	if (buffer == 0 || BufferAlive(buffer))
		return;

	if (!TryWriteDword(reinterpret_cast<void*>(static_cast<uintptr_t>(bufferField)), 0))
		return;

	const LONG dropped = InterlockedIncrement(&g_dropped);
	LOG("SoundHolderGuard: a sound player still pointed at buffer %08X, which DirectSound had already freed; "
		"it is left alone so the game can tear the player down (%ld so far)", buffer, dropped);
}

void NoteFirstTeardown()
{
	if (InterlockedExchange(&g_seen, 1) == 0)
		LOG("SoundHolderGuard: the game tore down its first sound player");
}

void __fastcall HookedStopSoundHolder(void** slot, void* unused)
{
	NoteFirstTeardown();
	DropFreedBuffer(slot);
	g_stopHook.Original()(slot, unused);
}

}

bool SoundHolderGuard::Install()
{
	const bool installed = g_stopHook.InstallRva(GameOffsets::kFnStopSoundHolder, &HookedStopSoundHolder);

	LOG("SoundHolderGuard: %s", installed ? "on" : "the sound player teardown could not be hooked");
	return installed;
}
