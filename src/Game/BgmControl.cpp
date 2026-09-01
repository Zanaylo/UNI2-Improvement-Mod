#include "Game/BgmControl.h"

#include "Core/CrashContext.h"
#include "Core/logger.h"
#include "Core/interfaces.h"
#include "Core/utils.h"
#include "Game/BgmCatalog.h"
#include "Game/BgmLibrary.h"
#include "Game/BgmTableFile.h"
#include "Game/BgmNames.h"
#include "Game/BgmRules.h"
#include "Game/BgmTable.h"
#include "Game/BgmVolume.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

using BgmPlay_t = bool(__fastcall*)(int, void*);
using BgmStop_t = void(__cdecl*)();
using BgmStart_t = void(__cdecl*)();
using BgmPause_t = void(__cdecl*)();
using MenuBgm_t = void(__fastcall*)(void*, void*, int);

enum State
{
	State_Stopped = 0,
	State_Playing = 1,
	State_Paused = 2,
};

BgmPlay_t oBgmPlay = nullptr;
BgmStart_t oBgmStart = nullptr;
BgmPause_t oBgmPause = nullptr;
BgmStop_t g_stop = nullptr;
MenuBgm_t oMenuBgm = nullptr;

bool g_hooked = false;
bool g_reported = false;

volatile long g_lastRequested = -1;
volatile long g_lastPlayed = -1;

int g_playing = -1;
int g_pinned = -1;
int g_menuTrack = -1;
bool g_inChooser = false;

uint64_t g_positionBase = 0;
float g_positionSeconds = 0.0f;
char g_positionFile[GameOffsets::kBgmFileMax + 1] = {};
bool g_positionHeld = false;

char g_status[256] = "not started";

void* GameFunction(uintptr_t rva)
{
	const uintptr_t address = RvaToAddress(rva);

	if (!IsAddressInGameModule(address))
		return nullptr;

	return reinterpret_cast<void*>(address);
}

uint32_t ReadGlobal(uintptr_t rva)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), value);
	return value;
}

void WriteGlobal(uintptr_t rva, uint32_t value)
{
	TryWriteDword(reinterpret_cast<void*>(RvaToAddress(rva)), value);
}

int SlotOf(int id)
{
	if (!BgmLibrary::IsLibraryId(id))
		return id;

	const int slot = BgmLibrary::SlotOf(id);
	return slot >= 0 ? slot : BgmLibrary::WindowSlot();
}

void SeekStream(void* stream, float seconds)
{
	const uintptr_t address = RvaToAddress(GameOffsets::kFnBgmSeek);

	if (stream == nullptr || !IsAddressInGameModule(address))
		return;

	__asm
	{
		movss xmm1, seconds
		mov ecx, stream
		mov eax, address
		call eax
	}
}

float PlayedSeconds()
{
	const uint64_t now = GetTickCount64();

	return now <= g_positionBase ? 0.0f : static_cast<float>((now - g_positionBase) / 1000.0);
}

void MarkPlayingFrom(float seconds)
{
	g_positionBase = GetTickCount64() - static_cast<uint64_t>(seconds * 1000.0f);
}

bool LoadedFile(char* out, int size)
{
	BgmTable::Entry entry = {};

	if (!BgmTable::Read(static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)), entry))
		return false;

	if (!entry.present || entry.file[0] == '\0')
		return false;

	strncpy_s(out, size, entry.file, _TRUNCATE);
	return true;
}

void HoldPosition()
{
	g_positionHeld = false;

	if (!g_modVals.keepMenuMusic || ReadGlobal(GameOffsets::kBgmPlayer) == 0)
		return;

	if (!LoadedFile(g_positionFile, sizeof(g_positionFile)))
		return;

	g_positionSeconds = PlayedSeconds();
	g_positionHeld = g_positionSeconds > 1.0f;
}

bool RestorePosition()
{
	if (!g_positionHeld || !g_modVals.keepMenuMusic)
		return false;

	g_positionHeld = false;

	if (ReadGlobal(GameOffsets::kBgmState) == State_Paused)
		return false;

	char loaded[GameOffsets::kBgmFileMax + 1] = {};

	if (!LoadedFile(loaded, sizeof(loaded)) || _stricmp(loaded, g_positionFile) != 0)
		return false;

	void* const stream = reinterpret_cast<void*>(ReadGlobal(GameOffsets::kBgmPlayer));

	if (stream == nullptr)
		return false;

	SeekStream(stream, g_positionSeconds);
	WriteGlobal(GameOffsets::kBgmState, State_Paused);
	MarkPlayingFrom(g_positionSeconds);

	LOG("BgmControl: '%s' picked up at %.1fs instead of the top", loaded,
		static_cast<double>(g_positionSeconds));

	return true;
}

bool SameTrackLoaded(int slot)
{
	const int loaded = static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId));

	if (loaded == slot)
		return true;

	BgmTable::Entry playing = {};
	BgmTable::Entry wanted = {};

	if (!BgmTable::Read(loaded, playing) || !BgmTable::Read(slot, wanted))
		return false;

	if (!playing.present || !wanted.present || playing.file[0] == '\0')
		return false;

	return _stricmp(playing.file, wanted.file) == 0;
}

bool StillLoaded(int id)
{
	if (id < 0)
		return false;

	if (ReadGlobal(GameOffsets::kBgmPlayer) == 0)
		return false;

	if (BgmLibrary::IsLibraryId(id) && id != g_playing)
		return false;

	return SameTrackLoaded(SlotOf(id));
}

bool AlreadyRunning()
{
	if (!g_modVals.keepMenuMusic)
		return false;

	if (ReadGlobal(GameOffsets::kBgmState) != State_Playing)
		return false;

	return StillLoaded(g_playing);
}

bool StartTrack(int id, void* edx)
{
	int slot = id;

	if (BgmLibrary::IsLibraryId(id))
	{
		slot = BgmLibrary::Bind(id);

		if (slot < 0)
			return false;
	}

	BgmVolume::ApplyToSlot(slot, id);

	const bool result = oBgmPlay(slot, edx);

	if (result)
	{
		g_playing = id;
		BgmVolume::SetCurrent(slot, id);
	}

	WriteGlobal(GameOffsets::kBgmState, State_Stopped);

	return result;
}

void ReportTableOnce()
{
	if (g_reported)
		return;

	g_reported = true;

	int vanilla = 0;
	int custom = 0;

	for (int id = 0; id < BgmTable::kSlotCount; ++id)
	{
		if (!BgmTable::IsPresent(id))
			continue;

		if (id < 100)
			++vanilla;
		else
			++custom;
	}

	LOG("BgmControl: %d vanilla slot(s), %d pack slot(s), %d library track(s), window slot %d",
		vanilla, custom, BgmLibrary::Count(), BgmLibrary::WindowSlot());
}

bool __fastcall HookedBgmPlay(int id, void* edx)
{
	g_lastRequested = id;
	ReportTableOnce();

	if (id < 0)
		return oBgmPlay(id, edx);

	if (g_pinned >= 0)
	{
		LOG("BgmControl: game asked for %d, held back by your pick", id);
		return true;
	}

	int asked = id;

	if (!g_inChooser)
		g_menuTrack = id;
	else if (g_modVals.keepMenuMusic && g_menuTrack >= 0 && g_menuTrack != id)
	{
		LOG("BgmControl: the menu chooser asked for %d, following the menu's own %d instead", id,
			g_menuTrack);

		asked = g_menuTrack;
	}

	const int left = GameState::GetLoadedCharacter(0);
	const int right = GameState::GetLoadedCharacter(1);

	int chosen = asked;

	if (BgmCatalog::ShuffleEnabled())
	{
		const int picked = BgmCatalog::Pick(g_lastPlayed);

		if (picked >= 0)
			chosen = picked;
	}
	else
	{
		const int resolved = BgmRules::Resolve(asked, left, right);

		if (resolved >= 0)
			chosen = resolved;
	}

	g_lastPlayed = chosen;

	const bool loaded = StillLoaded(chosen);

	BgmTable::Entry current = {};
	BgmTable::Read(static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)), current);

	LOG("BgmControl: game asked for %d (chara %d vs %d), playing %d, already loaded %d, slot %d "
		"holding '%s', state %u", id, left, right, chosen, loaded ? 1 : 0,
		static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)), current.file,
		ReadGlobal(GameOffsets::kBgmState));

	if (loaded)
		return true;

	return StartTrack(chosen, edx);
}

void __cdecl HookedBgmStart();

void __fastcall HookedMenuBgm(void* self, void* unused, int scene)
{
	const uint32_t stream = ReadGlobal(GameOffsets::kBgmPlayer);
	const uint32_t state = ReadGlobal(GameOffsets::kBgmState);
	const int before = static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId));

	const bool holding = g_modVals.keepMenuMusic && stream != 0 && before >= 0 &&
		state != State_Playing;

	LOG("BgmControl: menu chooser in, scene %d, remembered %d, current %d, state %u, stream %08X, "
		"holding %d", scene, static_cast<int>(ReadGlobal(GameOffsets::kMenuBgmRemembered)), before,
		state, stream, holding ? 1 : 0);

	if (holding)
		WriteGlobal(GameOffsets::kBgmState, State_Playing);

	g_inChooser = true;
	oMenuBgm(self, unused, scene);
	g_inChooser = false;

	const uint32_t kept = ReadGlobal(GameOffsets::kBgmPlayer);
	const int now = static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId));

	LOG("BgmControl: menu chooser out, current %d, state %u, stream %08X", now,
		ReadGlobal(GameOffsets::kBgmState), kept);

	if (!holding || kept == 0 || now != before)
		return;

	WriteGlobal(GameOffsets::kBgmState, State_Paused);
	HookedBgmStart();
}

void __cdecl HookedBgmStart()
{
	const uint32_t state = ReadGlobal(GameOffsets::kBgmState);

	if (AlreadyRunning())
	{
		LOG("BgmControl: start ignored, %d is already running at %.1fs on slot %d", g_playing,
			static_cast<double>(PlayedSeconds()),
			static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)));

		BgmVolume::ApplyNow();
		return;
	}

	const bool restored = RestorePosition();

	LOG("BgmControl: start, state %u, playing %d, slot %d, loaded %u, stream %08X - %s", state,
		g_playing, static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)),
		ReadGlobal(GameOffsets::kBgmLoadedFlag), ReadGlobal(GameOffsets::kBgmPlayer),
		restored ? "picking up where it left off"
		: state == State_Paused ? "resuming" : "from the top");

	if (!restored && state != State_Paused)
		MarkPlayingFrom(0.0f);
	else if (!restored)
		MarkPlayingFrom(g_positionSeconds);

	oBgmStart();
}

void __cdecl HookedBgmPause()
{
	const uint32_t before = ReadGlobal(GameOffsets::kBgmState);

	oBgmPause();

	const uint32_t after = ReadGlobal(GameOffsets::kBgmState);
	const uint32_t stream = ReadGlobal(GameOffsets::kBgmPlayer);
	const bool resume = before == State_Playing && after == State_Stopped && stream != 0 &&
		g_playing >= 0;

	if (before == State_Playing)
	{
		g_positionSeconds = PlayedSeconds();
		HoldPosition();
	}

	LOG("BgmControl: pause, state %u -> %u, playing %d, stream %08X, resumable %d, held %.1fs",
		before, after, g_playing, stream, resume ? 1 : 0,
		g_positionHeld ? static_cast<double>(g_positionSeconds) : 0.0);

	if (!resume)
		return;

	WriteGlobal(GameOffsets::kBgmState, State_Paused);
}

}

bool BgmControl::Initialize()
{
	BgmTableFile::Repair();

	if (g_hooked)
		return true;

	BgmLibrary::Load();
	BgmNames::Load();
	BgmRules::Load();
	BgmCatalog::Load();
	BgmVolume::Load();
	BgmVolume::Install();

	void* play = GameFunction(GameOffsets::kFnBgmPlay);
	void* pause = GameFunction(GameOffsets::kFnBgmPause);

	if (play == nullptr || pause == nullptr)
	{
		strncpy_s(g_status, "the BGM player is outside the game module", _TRUNCATE);
		LOG("BgmControl: %s", g_status);
		return false;
	}

	g_stop = reinterpret_cast<BgmStop_t>(GameFunction(GameOffsets::kFnBgmStop));

	void* const chooser = GameFunction(GameOffsets::kFnMenuBgmChoose);

	if (chooser != nullptr)
	{
		HookManager::CreateAndEnableHook(chooser, &HookedMenuBgm,
			reinterpret_cast<void**>(&oMenuBgm), "MenuBgmChoose");
	}

	void* const start = GameFunction(GameOffsets::kFnBgmStart);

	if (start != nullptr && !HookManager::CreateAndEnableHook(start, &HookedBgmStart,
		reinterpret_cast<void**>(&oBgmStart), "BgmStart"))
	{
		oBgmStart = reinterpret_cast<BgmStart_t>(start);
	}

	if (!HookManager::CreateAndEnableHook(play, &HookedBgmPlay,
		reinterpret_cast<void**>(&oBgmPlay), "BgmPlay"))
	{
		strncpy_s(g_status, "the BGM player could not be hooked", _TRUNCATE);
		LOG("BgmControl: %s", g_status);
		return false;
	}

	if (!HookManager::CreateAndEnableHook(pause, &HookedBgmPause,
		reinterpret_cast<void**>(&oBgmPause), "BgmPause"))
	{
		strncpy_s(g_status, "the BGM pause call could not be hooked", _TRUNCATE);
		LOG("BgmControl: %s", g_status);
		return false;
	}

	g_hooked = true;

	CrashContext::Register("BGM", &BgmControl::WriteCrashReport);

	_snprintf_s(g_status, _TRUNCATE, "hooked, %d rule(s), %s", BgmRules::Count(), BgmLibrary::StatusText());
	LOG("BgmControl: %s", g_status);
	return true;
}

bool BgmControl::IsHooked()
{
	return g_hooked;
}

void BgmControl::WriteCrashReport()
{
	LOG_RAW("  last requested: %d", GetLastRequested());
	LOG_RAW("  last played:    %d", GetLastPlayed());
	LOG_RAW("  now playing:    %d, pinned %d", g_playing, g_pinned);
	LOG_RAW("  state:          %d, stream %08X, game id %d", ReadGlobal(GameOffsets::kBgmState),
		ReadGlobal(GameOffsets::kBgmPlayer),
		static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)));
	LOG_RAW("  window slot:    %d, bound %d", BgmLibrary::WindowSlot(), BgmLibrary::BoundId());
	LOG_RAW("  characters:     %d vs %d", GetCharacter(0), GetCharacter(1));
	LOG_RAW("  rules enabled:  %d, count %d", BgmRules::IsEnabled() ? 1 : 0, BgmRules::Count());

	for (int i = 0; i < BgmRules::Count(); ++i)
	{
		const BgmRules::Rule* rule = BgmRules::Get(i);
		if (rule == nullptr)
			continue;

		LOG_RAW("  rule %d: kind=%d a=%d b=%d bgm=%d bothWays=%d enabled=%d theme=%d", i,
			rule->kind, rule->a, rule->b, rule->bgm, rule->bothWays ? 1 : 0,
			rule->enabled ? 1 : 0, rule->fromTheme ? 1 : 0);
	}
}

int BgmControl::Current()
{
	if (ReadGlobal(GameOffsets::kBgmPlayer) == 0)
		return -1;

	if (g_playing >= 0)
		return g_playing;

	return static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId));
}

void BgmControl::RefreshVolume()
{
	const int id = Current();

	if (id < 0)
		return;

	const int slot = SlotOf(id);

	BgmVolume::ApplyToSlot(slot, id);
	BgmVolume::SetCurrent(slot, id);
	BgmVolume::ApplyNow();
}

bool BgmControl::IsSuppressed()
{
	return ReadGlobal(GameOffsets::kBgmSuppressed) != 0;
}

void BgmControl::Stop()
{
	g_pinned = -1;
	g_playing = -1;

	BgmVolume::SetCurrent(-1, -1);

	if (g_stop == nullptr)
		return;

	g_stop();
}

void BgmControl::Release()
{
	if (g_pinned < 0)
		return;

	g_pinned = -1;

	const int wanted = GetLastRequested();

	if (wanted < 0 || oBgmPlay == nullptr)
		return;

	const int left = GameState::GetLoadedCharacter(0);
	const int right = GameState::GetLoadedCharacter(1);
	const int resolved = BgmRules::Resolve(wanted, left, right);

	if (StartTrack(resolved >= 0 ? resolved : wanted, nullptr))
		HookedBgmStart();
}

bool BgmControl::IsPinned()
{
	return g_pinned >= 0;
}

int BgmControl::PinnedId()
{
	return g_pinned;
}

bool BgmControl::Play(int id)
{
	if (!g_hooked || oBgmPlay == nullptr)
	{
		LOG("BgmControl: Play(%d) refused, the hook is not installed", id);
		return false;
	}

	if (!BgmLibrary::IsPlayable(id))
	{
		LOG("BgmControl: Play(%d) refused, there is no track behind that id", id);
		return false;
	}

	if (IsSuppressed())
	{
		LOG("BgmControl: Play(%d) refused, the game has BGM switched off", id);
		return false;
	}

	if (g_stop != nullptr)
		g_stop();

	g_playing = -1;
	g_pinned = id;

	const bool loaded = StartTrack(id, nullptr);

	if (loaded)
		HookedBgmStart();

	LOG("BgmControl: Play(%d) slot %d, loaded %d, state %d, stream %08X", id, SlotOf(id),
		loaded ? 1 : 0, ReadGlobal(GameOffsets::kBgmState),
		ReadGlobal(GameOffsets::kBgmPlayer));

	return loaded;
}

int BgmControl::GetLastRequested()
{
	return static_cast<int>(g_lastRequested);
}

int BgmControl::GetLastPlayed()
{
	return static_cast<int>(g_lastPlayed);
}

int BgmControl::GetCharacter(int side)
{
	return GameState::GetLoadedCharacter(side);
}

const char* BgmControl::GetStatusText()
{
	return g_status;
}
