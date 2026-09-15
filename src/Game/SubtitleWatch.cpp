#include "Game/SubtitleWatch.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/SubtitleText.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

typedef int(__fastcall* SePlayFn)(void*, void*, const char*, int, void*);
typedef void(__fastcall* SeTableLoadFn)(const char*, int);

constexpr int kFadeMs = 350;
constexpr int kMinHoldMs = 500;
constexpr int kMaxHoldMs = 8000;
constexpr int kDefaultHoldMs = 2600;
constexpr int kNoteLimit = 24;
constexpr int kGroupMax = 32;
constexpr int kFramesPerSecond = 60;

struct Slot
{
	char text[SubtitleTable::kTextMax];
	int chara;
	uint64_t started;
};

SePlayFn oSePlay = nullptr;
SeTableLoadFn oSeTableLoad = nullptr;

int g_slotChara[GameOffsets::kBattlePlayerSeSlots] = {};

SRWLOCK g_lock = SRWLOCK_INIT;
Slot g_slots[SubtitleWatch::kMaxShown] = {};

bool g_enabled = false;
int g_holdMs = kDefaultHoldMs;

volatile long g_heard = 0;
volatile long g_shown = 0;
int g_noted = 0;
char g_seen[kNoteLimit][kGroupMax] = {};

constexpr const char* kNoSoundManager =
	"the game's sound manager is not where this game version expects it";

char g_status[192] = {};

void Summarise()
{
	if (oSePlay == nullptr)
	{
		strncpy_s(g_status, kNoSoundManager, _TRUNCATE);
		return;
	}

	if (!g_enabled)
	{
		strncpy_s(g_status, "subtitles are off", _TRUNCATE);
		return;
	}

	strncpy_s(g_status, oSeTableLoad != nullptr ? "subtitles are on"
		: "the sound tables have no names, so no subtitle can be matched", _TRUNCATE);
}

int SlotOf(const char* group)
{
	if (group == nullptr || group[0] == 0)
		return -1;

	for (const char* c = group; *c != 0; ++c)
	{
		if (*c < '0' || *c > '9')
			return -1;
	}

	const int slot = atoi(group);

	return slot < GameOffsets::kBattlePlayerSeSlots ? slot : -1;
}

int CharaOf(const char* group)
{
	const int slot = SlotOf(group);

	return slot < 0 ? -1 : g_slotChara[slot];
}

int CharaOfFolder(const char* folder)
{
	if (folder == nullptr || _strnicmp(folder, "chr", 3) != 0)
		return -1;

	const int chara = atoi(folder + 3);

	return chara >= 0 && chara < SubtitleTable::kCharacters ? chara : -1;
}

void __fastcall HookedSeTableLoad(const char* folder, int slot)
{
	if (slot >= 0 && slot < GameOffsets::kBattlePlayerSeSlots)
	{
		g_slotChara[slot] = CharaOfFolder(folder);

		LOG("SubtitleWatch: sound slot %d now holds '%s', character %d", slot,
			folder != nullptr ? folder : "", g_slotChara[slot]);
	}

	oSeTableLoad(folder, slot);
}

void Note(const char* group, int index, int chara)
{
	for (int i = 0; i < g_noted; ++i)
	{
		if (strcmp(g_seen[i], group) == 0)
			return;
	}

	if (g_noted >= kNoteLimit)
		return;

	strncpy_s(g_seen[g_noted], group, _TRUNCATE);
	++g_noted;

	LOG("SubtitleWatch: sound group '%s' first seen at index %d, read as character %d, loaded "
		"%d vs %d", group, index, chara, GameState::GetLoadedCharacter(0),
		GameState::GetLoadedCharacter(1));
}

void Push(int chara, const char* text)
{
	AcquireSRWLockExclusive(&g_lock);

	int target = 0;
	uint64_t oldest = ~0ull;

	for (int i = 0; i < SubtitleWatch::kMaxShown; ++i)
	{
		if (g_slots[i].text[0] != 0 && strcmp(g_slots[i].text, text) == 0)
		{
			target = i;
			oldest = 0;
			break;
		}

		if (g_slots[i].started < oldest)
		{
			oldest = g_slots[i].started;
			target = i;
		}
	}

	strncpy_s(g_slots[target].text, text, _TRUNCATE);
	g_slots[target].chara = chara;
	g_slots[target].started = GetTickCount64();

	ReleaseSRWLockExclusive(&g_lock);

	InterlockedIncrement(&g_shown);
}

int __fastcall HookedSePlay(void* self, void* unused, const char* group, int index, void* params)
{
	const int played = oSePlay(self, unused, group, index, params);

	if (played == 0 || !g_enabled)
		return played;

	InterlockedIncrement(&g_heard);

	const int slot = SlotOf(group);
	const int chara = slot < 0 ? -1 : g_slotChara[slot];

	Note(group, index, chara);

	if (chara < 0 || SubtitleText::AnsweredRecently(chara, index))
		return played;

	if (SubtitleText::Draw(slot, chara, index, g_holdMs * kFramesPerSecond / 1000))
	{
		InterlockedIncrement(&g_shown);
		return played;
	}

	char text[SubtitleTable::kTextMax] = {};

	if (SubtitleTable::Lookup(chara, index, text, sizeof(text)))
		Push(chara, text);

	return played;
}

void InstallTableLoad()
{
	for (int i = 0; i < GameOffsets::kBattlePlayerSeSlots; ++i)
		g_slotChara[i] = -1;

	void* const target = reinterpret_cast<void*>(RvaToAddress(
		GameOffsets::kFnBattlePlayerSeLoad));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		LOG("SubtitleWatch: the battle sound table loader is not where this build expects it");
		return;
	}

	if (HookManager::CreateAndEnableHook(target, &HookedSeTableLoad,
		reinterpret_cast<void**>(&oSeTableLoad), "BattlePlayerSeLoad"))
	{
		return;
	}

	oSeTableLoad = nullptr;
	LOG("SubtitleWatch: the battle sound table loader could not be hooked");
}

}

bool SubtitleWatch::Install()
{
	if (oSePlay != nullptr)
		return true;

	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnSePlay));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		Summarise();
		LOG("SubtitleWatch: %s", g_status);
		return false;
	}

	if (!HookManager::CreateAndEnableHook(target, &HookedSePlay,
		reinterpret_cast<void**>(&oSePlay), "SeManagerPlay"))
	{
		oSePlay = nullptr;
		Summarise();
		LOG("SubtitleWatch: %s", g_status);
		return false;
	}

	InstallTableLoad();

	Summarise();
	LOG("SubtitleWatch: %s", g_status);
	return true;
}

bool SubtitleWatch::IsAvailable()
{
	return oSePlay != nullptr;
}

void SubtitleWatch::Update()
{
	if (!g_enabled)
		return;

	for (int slot = 0; slot < GameOffsets::kBattlePlayerSeSlots; ++slot)
	{
		const int chara = g_slotChara[slot];

		if (chara >= 0 && !SubtitleTable::IsAttached(chara))
			SubtitleTable::Attach(chara);
	}
}

bool SubtitleWatch::IsEnabled()
{
	return g_enabled;
}

void SubtitleWatch::SetEnabled(bool enabled)
{
	if (enabled == g_enabled)
		return;

	g_enabled = enabled;

	if (!enabled)
	{
		AcquireSRWLockExclusive(&g_lock);
		memset(g_slots, 0, sizeof(g_slots));
		ReleaseSRWLockExclusive(&g_lock);
	}

	Summarise();
}

int SubtitleWatch::HoldMs()
{
	return g_holdMs;
}

void SubtitleWatch::SetHoldMs(int milliseconds)
{
	if (milliseconds < kMinHoldMs)
		milliseconds = kMinHoldMs;

	if (milliseconds > kMaxHoldMs)
		milliseconds = kMaxHoldMs;

	g_holdMs = milliseconds;
}

bool SubtitleWatch::HasAny()
{
	if (!g_enabled)
		return false;

	const uint64_t now = GetTickCount64();
	const uint64_t life = static_cast<uint64_t>(g_holdMs) + kFadeMs;

	for (int i = 0; i < kMaxShown; ++i)
	{
		if (g_slots[i].text[0] != 0 && now - g_slots[i].started < life)
			return true;
	}

	return false;
}

int SubtitleWatch::Take(Shown* out, int max)
{
	if (out == nullptr || max <= 0 || !g_enabled)
		return 0;

	const uint64_t now = GetTickCount64();
	const uint64_t life = static_cast<uint64_t>(g_holdMs) + kFadeMs;

	int taken = 0;
	uint64_t order[kMaxShown] = {};

	AcquireSRWLockShared(&g_lock);

	for (int i = 0; i < kMaxShown && taken < max; ++i)
	{
		const Slot& slot = g_slots[i];

		if (slot.text[0] == 0)
			continue;

		const uint64_t age = now - slot.started;

		if (age >= life)
			continue;

		const uint64_t left = life - age;

		int at = taken;

		while (at > 0 && order[at - 1] > slot.started)
		{
			out[at] = out[at - 1];
			order[at] = order[at - 1];
			--at;
		}

		out[at].fade = left >= kFadeMs ? 1.0f : static_cast<float>(left) / kFadeMs;
		out[at].chara = slot.chara;
		strncpy_s(out[at].text, slot.text, _TRUNCATE);
		order[at] = slot.started;
		++taken;
	}

	ReleaseSRWLockShared(&g_lock);

	return taken;
}

int SubtitleWatch::Heard()
{
	return static_cast<int>(g_heard);
}

int SubtitleWatch::Shows()
{
	return static_cast<int>(g_shown);
}

const char* SubtitleWatch::StatusText()
{
	return g_status;
}
