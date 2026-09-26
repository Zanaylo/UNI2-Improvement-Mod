#include "Game/Audio/BgmControl.h"

#include "Core/Boot/CrashContext.h"
#include "Core/logger.h"
#include "Core/Config/interfaces.h"
#include "Core/utils.h"
#include "Game/Audio/BgmCatalog.h"
#include "Game/Audio/BgmLibrary.h"
#include "Game/Audio/BgmTableFile.h"
#include "Game/Audio/BgmNames.h"
#include "Game/Audio/BgmRules.h"
#include "Game/Audio/BgmTable.h"
#include "Game/Audio/BgmVolume.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/SceneWatch.h"
#include "Hooks/GameHook.h"
#include "Game/Engine/CodeSignatures.h"

#include <Windows.h>

#include <cstdarg>
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

GameHook<BgmPlay_t> g_bgmPlayHook("BgmPlay");
GameHook<BgmStart_t> g_bgmStartHook("BgmStart");
BgmStart_t g_bgmStartUnhooked = nullptr;
GameHook<BgmPause_t> g_bgmPauseHook("BgmPause");
BgmStop_t g_stop = nullptr;
GameHook<MenuBgm_t> g_menuBgmHook("MenuBgmChoose");

constexpr int kIdleStartGrace = 4;
constexpr int kIdleStartPeriod = 30;
constexpr uint64_t kSelfStartGraceMs = 250;
constexpr int kHeldPauseFrames = 8;

bool g_hooked = false;
bool g_reported = false;

volatile long g_lastRequested = -1;
volatile long g_lastPlayed = -1;

int g_playing = -1;
int g_pinned = -1;
int g_menuTrack = -1;
int g_shuffleAsked = -1;
int g_shufflePick = -1;
int g_takenOver = -1;
bool g_inChooser = false;
bool g_inBattle = false;
uint32_t g_startedStream = 0;
int g_heldPauseFrames = 0;
bool g_carryStart = false;

uint64_t g_positionBase = 0;
float g_positionSeconds = 0.0f;
char g_positionFile[GameOffsets::kBgmFileMax + 1] = {};
bool g_positionHeld = false;

int g_idleSlot = -1;
int g_idleStarts = 0;

uint64_t g_selfStartedAt = 0;
uint32_t g_pinnedScene = SceneWatch::kNone;

char g_reason[128] = "the game's own";

char g_status[256] = "not started";

void* GameFunction(uintptr_t rva)
{
	const uintptr_t address = CodeSignatures::Address(rva);

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

const char* SlotName(int id)
{
	const char* const name = BgmTable::DescribeSlot(id);

	return name != nullptr && name[0] != 0 ? name : "track";
}

void Explain(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(g_reason, sizeof(g_reason), format, args);
	va_end(args);
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
	const uintptr_t address = CodeSignatures::Address(GameOffsets::kFnBgmSeek);

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

bool g_parkedPause = false;

void ParkPaused(const char* why)
{
	if (ReadGlobal(GameOffsets::kBgmPlayer) == 0)
	{
		LOG("BgmControl: the pause after %s was not parked, the stream is already gone", why);
		return;
	}

	WriteGlobal(GameOffsets::kBgmState, State_Paused);
	g_parkedPause = true;
}

void ClearOrphanPause()
{
	if (!g_parkedPause)
		return;

	if (ReadGlobal(GameOffsets::kBgmPlayer) != 0)
		return;

	g_parkedPause = false;

	if (ReadGlobal(GameOffsets::kBgmState) != State_Paused)
		return;

	WriteGlobal(GameOffsets::kBgmState, State_Stopped);
	LOG("BgmControl: the parked pause outlived its stream, the state is cleared");
}

void StopStream()
{
	g_parkedPause = false;

	if (g_stop == nullptr || ReadGlobal(GameOffsets::kBgmPlayer) == 0)
		return;

	g_stop();
}

void HoldPosition()
{
	g_positionHeld = false;

	if (!g_modVals.keepMenuMusic || ReadGlobal(GameOffsets::kBgmPlayer) == 0)
		return;

	if (GameState::IsInMatch())
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
	ParkPaused("the pick-up");
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

	const bool result = g_bgmPlayHook.Original()(slot, edx);

	if (result)
	{
		g_playing = id;
		BgmVolume::SetCurrent(slot, id);
	}

	g_startedStream = 0;
	g_parkedPause = false;
	g_carryStart = false;
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

int Shuffled(int asked)
{
	if (asked == g_shuffleAsked && g_shufflePick >= 0)
		return g_shufflePick;

	const int picked = BgmCatalog::Pick(g_lastPlayed);

	if (picked < 0)
		return asked;

	g_shuffleAsked = asked;
	g_shufflePick = picked;

	LOG("BgmControl: the randomizer drew %d for scene %d", picked, asked);

	return picked;
}

void ForgetDrawAfterBattle()
{
	const uint32_t scene = SceneWatch::Current();

	if (scene == SceneWatch::kNone)
		return;

	const bool inBattle = scene == GameOffsets::kSceneBattle;
	const bool left = g_inBattle && !inBattle;

	g_inBattle = inBattle;

	if (!left || g_shufflePick < 0)
		return;

	LOG("BgmControl: the battle is over, so the next match draws a new track instead of %d",
		g_shufflePick);

	g_shuffleAsked = -1;
	g_shufflePick = -1;
}

void CarryOnThroughRound(int asked, int chosen, uint32_t state)
{
	if (chosen == asked || state != State_Stopped || !GameState::IsInMatch())
		return;

	const uint32_t stream = ReadGlobal(GameOffsets::kBgmPlayer);

	if (stream == 0 || stream != g_startedStream)
		return;

	ParkPaused("the round change");

	LOG("BgmControl: the game asked for its own %d again mid-match, so %d carries on instead of "
		"starting over", asked, chosen);
}

bool SwappedTrackInMatch()
{
	if (!GameState::IsInMatch() || g_playing < 0 || g_lastPlayed == g_lastRequested)
		return false;

	if (ReadGlobal(GameOffsets::kBgmState) != State_Playing)
		return false;

	const uint32_t stream = ReadGlobal(GameOffsets::kBgmPlayer);

	return stream != 0 && stream == g_startedStream;
}

void RunHeldPause(const char* why)
{
	g_heldPauseFrames = 0;

	if (ReadGlobal(GameOffsets::kBgmPlayer) == 0)
	{
		WriteGlobal(GameOffsets::kBgmState, State_Stopped);
		LOG("BgmControl: the held pause found its stream gone, the state is cleared");
		return;
	}

	g_bgmPauseHook.Original()();

	LOG("BgmControl: the held pause went through, %s", why);
}

void ReleaseHeldPause(const char* why)
{
	if (g_heldPauseFrames == 0)
		return;

	RunHeldPause(why);
}

void ExpireHeldPause()
{
	if (g_heldPauseFrames == 0)
		return;

	if (--g_heldPauseFrames > 0 && ReadGlobal(GameOffsets::kBgmPlayer) != 0)
		return;

	RunHeldPause("nothing asked for the track again");
}

bool AbsorbHeldPause(int asked)
{
	if (g_heldPauseFrames == 0)
		return false;

	g_heldPauseFrames = 0;
	g_carryStart = true;

	LOG("BgmControl: the game asked for its own %d again mid-match, so %d plays on through the "
		"round change without a pause", asked, g_playing);

	return true;
}

void __cdecl HookedBgmStart();

void SelfStart()
{
	g_selfStartedAt = GetTickCount64();
	HookedBgmStart();
}

bool GameStartIsRedundant()
{
	if (g_selfStartedAt == 0)
		return false;

	if (GetTickCount64() - g_selfStartedAt > kSelfStartGraceMs)
	{
		g_selfStartedAt = 0;
		return false;
	}

	if (ReadGlobal(GameOffsets::kBgmState) != State_Playing)
		return false;

	return StillLoaded(g_playing);
}

bool __fastcall HookedBgmPlay(int id, void* edx)
{
	g_lastRequested = id;
	ReportTableOnce();

	if (id < 0)
		return g_bgmPlayHook.Original()(id, edx);

	if (g_pinned >= 0 && !BgmCatalog::ShuffleEnabled())
	{
		AbsorbHeldPause(id);
		Explain("your pick, held over the %s the game asked for", SlotName(id));
		LOG("BgmControl: game asked for %d, held back by your pick", id);
		return true;
	}

	if (g_pinned >= 0)
	{
		LOG("BgmControl: the randomizer is on, so your pick of %d is let go", g_pinned);
		g_pinned = -1;
	}

	int asked = id;

	if (!g_inChooser)
		g_menuTrack = id;
	else if (g_modVals.keepMenuMusic && g_menuTrack >= 0 && g_menuTrack != id)
	{
		const bool ownTrack = BgmTable::IsPresent(id);

		LOG("BgmControl: the menu chooser asked for %d while the menu had %d, slot %s", id,
			g_menuTrack, ownTrack ? "has a track of its own - following it"
			: "is empty - keeping the menu's");

		if (!ownTrack)
			asked = g_menuTrack;
	}

	const int left = GameState::GetLoadedCharacter(0);
	const int right = GameState::GetLoadedCharacter(1);

	int chosen = asked;

	if (BgmCatalog::ShuffleEnabled())
	{
		chosen = Shuffled(asked);
		Explain("the randomizer, over the %s the game asked for", SlotName(asked));
	}
	else
	{
		const int resolved = BgmRules::Resolve(asked, left, right);

		if (resolved < 0)
		{
			Explain("the game's own %s", SlotName(asked));
		}
		else
		{
			chosen = resolved;
			Explain("a rule of yours on the %s", SlotName(asked));
		}
	}

	g_lastPlayed = chosen;

	const bool loaded = StillLoaded(chosen);

	BgmTable::Entry current = {};
	BgmTable::Read(static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)), current);

	const uint32_t state = ReadGlobal(GameOffsets::kBgmState);

	LOG("BgmControl: game asked for %d (chara %d vs %d), playing %d, already loaded %d, slot %d "
		"holding '%s', state %u", id, left, right, chosen, loaded ? 1 : 0,
		static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)), current.file, state);

	if (loaded)
	{
		if (!AbsorbHeldPause(id))
			CarryOnThroughRound(id, chosen, state);

		return true;
	}

	ReleaseHeldPause("a different track was asked for");

	if (!StartTrack(chosen, edx))
		return false;

	if (state == State_Playing)
		SelfStart();

	return true;
}

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
	g_menuBgmHook.Original()(self, unused, scene);
	g_inChooser = false;

	const uint32_t kept = ReadGlobal(GameOffsets::kBgmPlayer);
	const int now = static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId));

	LOG("BgmControl: menu chooser out, current %d, state %u, stream %08X", now,
		ReadGlobal(GameOffsets::kBgmState), kept);

	if (!holding || kept == 0 || now != before)
		return;

	ParkPaused("the menu chooser");
	HookedBgmStart();
}

bool StreamIdle()
{
	if (ReadGlobal(GameOffsets::kBgmPlayer) != 0)
		return false;

	return ReadGlobal(GameOffsets::kBgmLoadedFlag) == 0;
}

bool SkipIdleStart(int slot)
{
	if (!StreamIdle())
	{
		g_idleSlot = -1;
		g_idleStarts = 0;
		return false;
	}

	if (slot != g_idleSlot)
	{
		g_idleSlot = slot;
		g_idleStarts = 0;
	}

	++g_idleStarts;

	if (g_idleStarts <= kIdleStartGrace)
		return false;

	return g_idleStarts % kIdleStartPeriod != 0;
}

bool CarriedStart()
{
	if (!g_carryStart)
		return false;

	g_carryStart = false;

	return ReadGlobal(GameOffsets::kBgmState) == State_Playing && StillLoaded(g_playing);
}

void __cdecl HookedBgmStart()
{
	if (CarriedStart())
	{
		LOG("BgmControl: start ignored, %d kept playing through the round change", g_playing);
		BgmVolume::ApplyNow();
		return;
	}

	if (GameStartIsRedundant())
	{
		g_selfStartedAt = 0;
		LOG("BgmControl: start ignored, the mod started %d a moment ago", g_playing);
		return;
	}

	const uint32_t state = ReadGlobal(GameOffsets::kBgmState);

	if (AlreadyRunning())
	{
		LOG("BgmControl: start ignored, %d is already running at %.1fs on slot %d", g_playing,
			static_cast<double>(PlayedSeconds()),
			static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId)));

		BgmVolume::ApplyNow();
		return;
	}

	if (SkipIdleStart(static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId))))
		return;

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

	g_startedStream = ReadGlobal(GameOffsets::kBgmPlayer);
	(g_bgmStartHook.IsLive() ? g_bgmStartHook.Original() : g_bgmStartUnhooked)();
}

bool MayHoldPaused()
{
	if (!g_modVals.keepMenuMusic)
		return false;

	return !GameState::IsInMatch();
}

bool HoldRoundPause()
{
	if (g_heldPauseFrames > 0)
		return true;

	if (!SwappedTrackInMatch())
		return false;

	g_heldPauseFrames = kHeldPauseFrames;

	LOG("BgmControl: pause held, %d is the mod's own track mid-match and the game is about to "
		"ask for its %d again", g_playing, static_cast<int>(g_lastRequested));

	return true;
}

void __cdecl HookedBgmPause()
{
	if (HoldRoundPause())
		return;

	const uint32_t before = ReadGlobal(GameOffsets::kBgmState);

	g_bgmPauseHook.Original()();

	const uint32_t after = ReadGlobal(GameOffsets::kBgmState);
	const uint32_t stream = ReadGlobal(GameOffsets::kBgmPlayer);
	const bool resume = MayHoldPaused() && before == State_Playing && after == State_Stopped &&
		stream != 0 && g_playing >= 0;

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

	ParkPaused("the game's own pause");
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
		strncpy_s(g_status, "the music player is not where this game version expects it", _TRUNCATE);
		LOG("BgmControl: %s", g_status);
		return false;
	}

	g_stop = reinterpret_cast<BgmStop_t>(GameFunction(GameOffsets::kFnBgmStop));

	void* const chooser = GameFunction(GameOffsets::kFnMenuBgmChoose);

	if (chooser != nullptr)
		g_menuBgmHook.Install(chooser, &HookedMenuBgm);

	void* const start = GameFunction(GameOffsets::kFnBgmStart);

	if (start != nullptr && !g_bgmStartHook.Install(start, &HookedBgmStart))
		g_bgmStartUnhooked = reinterpret_cast<BgmStart_t>(start);

	if (!g_bgmPlayHook.Install(play, &HookedBgmPlay))
	{
		strncpy_s(g_status, "the music player could not be hooked", _TRUNCATE);
		LOG("BgmControl: %s", g_status);
		return false;
	}

	if (!g_bgmPauseHook.Install(pause, &HookedBgmPause))
	{
		strncpy_s(g_status, "music pause could not be hooked", _TRUNCATE);
		LOG("BgmControl: %s", g_status);
		return false;
	}

	g_hooked = true;

	CrashContext::Register("BGM", &BgmControl::WriteCrashReport);

	_snprintf_s(g_status, _TRUNCATE, "on, %d rule(s), %s", BgmRules::Count(), BgmLibrary::StatusText());
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

	const int loaded = static_cast<int>(ReadGlobal(GameOffsets::kBgmCurrentId));

	if (g_playing < 0)
		return loaded;

	if (StillLoaded(g_playing))
		return g_playing;

	if (g_takenOver != loaded)
	{
		g_takenOver = loaded;
		LOG("BgmControl: %d was started but the stream holds slot %d - something changed the "
			"track without asking", g_playing, loaded);
	}

	return loaded;
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
	g_positionHeld = false;

	BgmVolume::SetCurrent(-1, -1);

	StopStream();
}

void BgmControl::Release()
{
	if (g_pinned < 0)
		return;

	g_pinned = -1;

	const int wanted = GetLastRequested();

	if (wanted < 0 || !g_bgmPlayHook.IsLive())
		return;

	const int left = GameState::GetLoadedCharacter(0);
	const int right = GameState::GetLoadedCharacter(1);
	const int resolved = BgmRules::Resolve(wanted, left, right);

	if (StartTrack(resolved >= 0 ? resolved : wanted, nullptr))
		HookedBgmStart();
}

void BgmControl::Reshuffle()
{
	g_pinned = -1;
	g_shuffleAsked = -1;
	g_shufflePick = -1;

	const int wanted = GetLastRequested();

	if (wanted < 0 || !g_bgmPlayHook.IsLive())
		return;

	const int picked = Shuffled(wanted);

	if (picked < 0)
		return;

	g_lastPlayed = picked;

	if (StartTrack(picked, nullptr))
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

void BgmControl::OnFrame()
{
	ExpireHeldPause();
	ClearOrphanPause();
	ForgetDrawAfterBattle();

	if (g_pinned < 0)
		return;

	const uint32_t scene = SceneWatch::Current();

	if (scene == SceneWatch::kNone || scene == g_pinnedScene)
		return;

	LOG("BgmControl: the screen changed, so your pick of %d is let go", g_pinned);
	Release();
}

const char* BgmControl::ReasonText()
{
	return g_reason;
}

bool BgmControl::Play(int id)
{
	if (!g_hooked || !g_bgmPlayHook.IsLive())
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

	StopStream();

	g_playing = -1;
	g_pinned = id;
	g_pinnedScene = SceneWatch::Current();

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
