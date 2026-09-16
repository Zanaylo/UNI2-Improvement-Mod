#include "Game/SubtitleText.h"

#include "Core/TextEncoding.h"
#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/CharaTables.h"
#include "Game/GameOffsets.h"
#include "Game/SubtitleTable.h"
#include "Game/SubtitleWatch.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace {

typedef const char*(__cdecl* ExStringTextFn)(const char*);

constexpr char kTag[] = "battlevoice_chr";
constexpr int kTagLength = sizeof(kTag) - 1;
constexpr unsigned int kGameCodePage = 1252;
constexpr long kAnswers = 32;
constexpr int kAnswerMax = 320;
constexpr long kReported = 24;
constexpr long kRecent = 8;
constexpr uint64_t kRecentMs = 4000;
constexpr int kMessageX = 640;
constexpr int kFirstSideY = 620;
constexpr int kSecondSideY = 650;
constexpr int kNoSound = -1;
constexpr int kCommonMessage = 0;
constexpr uintptr_t kSideAt = 4;
constexpr uintptr_t kParentAt = GameOffsets::kCharaOwner;
constexpr long kDrawsReported = 16;

struct Recent
{
	int chara;
	int index;
	uint64_t at;
};

ExStringTextFn oExStringText = nullptr;

volatile long g_answered = 0;
volatile long g_asked = 0;
volatile long g_next = 0;
volatile long g_recentNext = 0;
volatile long g_draws = 0;
bool g_drawing = false;
char g_answer[kAnswers][kAnswerMax] = {};
Recent g_recent[kRecent] = {};

char g_status[176] = "the game's string table is not where this game version expects it";

bool Named(const char* key, int& chara, int& index)
{
	if (key == nullptr || strncmp(key, kTag, kTagLength) != 0)
		return false;

	const char* at = key + kTagLength;

	if (*at < '0' || *at > '9')
		return false;

	chara = 0;

	while (*at >= '0' && *at <= '9')
		chara = chara * 10 + (*at++ - '0');

	if (*at != '_')
		return false;

	++at;

	if (*at < '0' || *at > '9')
		return false;

	index = 0;

	while (*at >= '0' && *at <= '9')
		index = index * 10 + (*at++ - '0');

	return *at == 0;
}

const char* Written(int chara, int index)
{
	char text[SubtitleTable::kTextMax] = {};

	if (!SubtitleTable::Lookup(chara, index, text, sizeof(text)))
		return nullptr;

	const char* const name = g_modVals.subtitleNames ? CharaTables::Name(chara) : nullptr;

	std::string line;

	if (name != nullptr)
	{
		line = name;
		line += ": ";
	}

	line += text;

	std::string encoded;

	if (!TextEncoding::Utf8ToCodePage(line, kGameCodePage, encoded, nullptr))
		return nullptr;

	char* const slot = g_answer[InterlockedIncrement(&g_next) & (kAnswers - 1)];

	strncpy_s(slot, kAnswerMax, encoded.c_str(), _TRUNCATE);
	InterlockedIncrement(&g_answered);

	return slot;
}

void Remember(int chara, int index)
{
	Recent& slot = g_recent[InterlockedIncrement(&g_recentNext) & (kRecent - 1)];

	slot.chara = chara;
	slot.index = index;
	slot.at = GetTickCount64();
}

void Report(const char* key, int chara, const char* written)
{
	if (InterlockedIncrement(&g_asked) > kReported)
		return;

	if (written != nullptr)
	{
		LOG("SubtitleText: the game asked for %s and was given the mod's line", key);
		return;
	}

	LOG("SubtitleText: the game asked for %s and kept its own - %s", key,
		SubtitleTable::IsAttached(chara) ? "no line is written for that voice"
		: "that character's lines are not loaded");
}

__declspec(naked) int __cdecl CallSetMessage(void*, void*, int, int, const char*, int, int, int)
{
	__asm
	{
		push ebp
		mov ebp, esp
		push ebx
		push dword ptr [ebp + 0x24]
		push dword ptr [ebp + 0x0c]
		push dword ptr [ebp + 0x20]
		push dword ptr [ebp + 0x1c]
		push dword ptr [ebp + 0x18]
		push dword ptr [ebp + 0x14]
		mov edx, dword ptr [ebp + 0x10]
		mov ecx, dword ptr [ebp + 0x0c]
		mov ebx, dword ptr [ebp + 0x08]
		call ebx
		add esp, 0x18
		pop ebx
		pop ebp
		ret
	}
}

uintptr_t ScriptSpeaker()
{
	uint32_t base = 0;
	uint32_t top = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kCharaStackBase)), base) ||
		!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kCharaStackTop)), top))
	{
		return 0;
	}

	if (top < base + 8)
		return 0;

	uint32_t chara = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(static_cast<uintptr_t>(top) - 8), chara) ||
		chara == 0)
	{
		return 0;
	}

	uint32_t parent = 0;

	if (TryReadDword(reinterpret_cast<const void*>(static_cast<uintptr_t>(chara) + kParentAt), parent) &&
		parent != 0)
	{
		chara = parent;
	}

	return chara;
}

void ReportDraw(const char* what, int chara, int index)
{
	if (InterlockedIncrement(&g_draws) > kDrawsReported)
		return;

	LOG("SubtitleText: voice chr%03d_%03d %s", chara, index, what);
}

const char* __cdecl HookedExStringText(const char* key)
{
	int chara = 0;
	int index = 0;

	if (!SubtitleWatch::IsEnabled() || !Named(key, chara, index))
		return oExStringText(key);

	const char* const written = Written(chara, index);

	Report(key, chara, written);

	if (written == nullptr)
		return oExStringText(key);

	Remember(chara, index);
	return written;
}

}

bool SubtitleText::Install()
{
	if (oExStringText != nullptr)
		return true;

	void* const target = reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnExStringText));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		LOG("SubtitleText: %s", g_status);
		return false;
	}

	if (!HookManager::CreateAndEnableHook(target, &HookedExStringText,
		reinterpret_cast<void**>(&oExStringText), "ExStringText"))
	{
		oExStringText = nullptr;
		LOG("SubtitleText: the game's string table could not be hooked");
		return false;
	}

	strncpy_s(g_status, "on, the game shows mod lines like its own", _TRUNCATE);
	LOG("SubtitleText: %s", g_status);

	return true;
}

bool SubtitleText::IsAvailable()
{
	return oExStringText != nullptr;
}

bool SubtitleText::AnsweredRecently(int chara, int index)
{
	const uint64_t now = GetTickCount64();

	for (const Recent& recent : g_recent)
	{
		if (recent.at != 0 && recent.chara == chara && recent.index == index &&
			now - recent.at < kRecentMs)
		{
			return true;
		}
	}

	return false;
}

bool SubtitleText::Draw(int slot, int chara, int index, int frames)
{
	if (g_drawing || slot < 0)
		return false;

	const uintptr_t target = RvaToAddress(GameOffsets::kFnCharaSetMessage);

	if (!IsAddressInGameModule(target))
		return false;

	const uintptr_t speaker = ScriptSpeaker();

	if (speaker == 0)
	{
		ReportDraw("played with no character running a script, the overlay writes it", chara, index);
		return false;
	}

	uint8_t side = 0xff;

	if (!TryReadMemory(&side, reinterpret_cast<const void*>(speaker + kSideAt), sizeof(side)) ||
		side != slot)
	{
		ReportDraw("played while another character's script was running, the overlay writes it",
			chara, index);
		return false;
	}

	const char* const text = Written(chara, index);

	if (text == nullptr)
		return false;

	g_drawing = true;

	CallSetMessage(reinterpret_cast<void*>(target), reinterpret_cast<void*>(speaker), kMessageX,
		side == 0 ? kFirstSideY : kSecondSideY, text, frames, kNoSound, kCommonMessage);

	g_drawing = false;

	ReportDraw("handed to the game's message drawer", chara, index);
	return true;
}

int SubtitleText::Answers()
{
	return static_cast<int>(g_answered);
}

int SubtitleText::Asked()
{
	return static_cast<int>(g_asked);
}

const char* SubtitleText::StatusText()
{
	return g_status;
}
