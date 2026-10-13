#include "Game/Debug/GameDebug.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/FileIndex.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Battle/BattleDataReload.h"
#include "Game/Debug/ScriptConsole.h"
#include "Game/Debug/ScriptDefines.h"
#include "Game/Files/DataArchive.h"
#include "Network/OnlineContext.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr const char* kSection = "GameDebug";
constexpr const char* kArchiveFolder = "script";
constexpr const char* kShippedDefines = "btl_Define_NoLocalDebug.txt";
constexpr const char* kServedKey = "script\\btl_Define_LocalDebug.txt";
constexpr const char* kGeneratedFile = "Generated\\btl_Define_LocalDebug.txt";
constexpr int kMostConstants = 2;
constexpr int kContextEveryFrames = 30;
constexpr int kOn = 1;

struct Entry
{
	GameDebug::Option option;
	const char* constants[kMostConstants];
};

const Entry kEntries[] = {
	{ { &g_modVals.gameDebugScriptLog, "ScriptLog", "Write the battle scripts' debug messages to the log",
		"The battle scripts carry messages French Bread used while testing. This writes them to the mod's log "
		"as [script] lines. The logs below need it.", false },
		{ "Def_Dbg_DebugMessage" } },
	{ { &g_modVals.gameDebugThrowLog, "ThrowLog", "Log throws and throw escapes",
		"Every throw that connects, and whether the escape came in time or in the miss window.", false },
		{ "Def_Dbg_SkillThrowLog", "Def_Dbg_TechHitLog" } },
	{ { &g_modVals.gameDebugFollowUpLog, "FollowUpLog", "Log missed follow-ups",
		"After a hit, says how many frames earlier a follow-up would have connected.", false },
		{ "Def_Dbg_ComboChanceLog" } },
	{ { &g_modVals.gameDebugRoundLog, "RoundLog", "Log round results",
		"How each round was won: KO, time up, and so on.", false },
		{ "Def_Dbg_RoundStatus" } },
	{ { &g_modVals.gameDebugSmartSteerLog, "SmartSteerLog", "Log Smart Steer",
		"Each step of the auto combo and why it continued or stopped.", false },
		{ "Def_Dbg_SmartSteerLog" } },
	{ { &g_modVals.gameDebugInvulnerabilityLog, "InvulnerabilityLog", "Log attribute invulnerability",
		"Which attribute a move was invulnerable to when it was hit.", false },
		{ "Def_Dbg_HitMutekiLog" } },
	{ { &g_modVals.gameDebugSoundLog, "SoundLog", "Log every sound effect",
		"Each sound effect a move plays, with its number and the move's frame.", false },
		{ "Def_DbgMes_PlaySE" } },
	{ { &g_modVals.gameDebugTechMargin, "TechMargin", "Show the throw escape margin",
		"When a throw is escaped, the attack info text shows how many frames were left, as TechSafe: 5F.", false },
		{ "Def_Dbg_LocalAnnounce" } },
	{ { &g_modVals.gameDebugQuietTaunts, "QuietTaunts", "Mute taunts and the GRD win sound",
		"Turns off the taunt voices and the sound when a GRD cycle is won. Game default: on.", false },
		{ "Def_Dbg_NoPlayUpsetSE" } },
	{ { &g_modVals.gameDebugCharaReadouts, "CharaReadouts", "Developer readouts for some characters",
		"A few characters draw their hidden values on screen, and the scripts check their own tables while "
		"they load. It also changes the CPU's choices, so it is never used online.", true },
		{ "Def_Dbg_LocalDebugMode" } },
	{ { &g_modVals.gameDebugAchievements, "Achievements", "Show achievements as they unlock",
		"Writes the achievement slot on screen the moment the game unlocks it. Never used online.", true },
		{ "Def_Dbg_AchiveMessage", "Def_Dbg_LocalDebugMode" } },
	{ { &g_modVals.gameDebugCancelAnything, "CancelAnything", "Normal attacks cancel anything",
		"Any normal attack can be used at any time, even during another move. Never used online.", true },
		{ "Def_Dbg_AllMoveAddCommand" } },
};

constexpr int kEntryCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));

std::string g_shipped;
std::string g_written;
bool g_served = false;
bool g_allowGameChanges = false;
bool g_reloadWanted = false;
int g_frame = 0;
char g_status[192] = "off";

std::string GeneratedPath()
{
	return GetModRootPath(kGeneratedFile);
}

bool AllowGameChanges()
{
	return !OnlineContext::IsActive() && BattleDataReload::IsSupported();
}

std::vector<ScriptDefines::Value> WantedValues()
{
	std::vector<ScriptDefines::Value> values;

	for (const Entry& entry : kEntries)
	{
		if (!*entry.option.value || (entry.option.changesTheGame && !g_allowGameChanges))
			continue;

		for (const char* constant : entry.constants)
		{
			if (constant != nullptr)
				values.push_back({ constant, kOn });
		}
	}

	return values;
}

bool ReadShipped()
{
	if (!g_shipped.empty())
		return true;

	std::vector<uint8_t> bytes;

	if (!DataArchive::Read(kArchiveFolder, kShippedDefines, bytes) || bytes.empty())
		return false;

	g_shipped.assign(bytes.begin(), bytes.end());
	return true;
}

bool WriteText(const std::string& path, const std::string& text)
{
	const std::string folder = path.substr(0, path.find_last_of('\\'));
	CreateDirectoryA(folder.c_str(), nullptr);

	const HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
		nullptr);

	if (file == INVALID_HANDLE_VALUE)
		return false;

	DWORD written = 0;
	const bool complete = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
		written == text.size();
	CloseHandle(file);

	return complete;
}

bool Regenerate()
{
	if (!ReadShipped())
	{
		strncpy_s(g_status, "the game's script defines could not be read, nothing is changed", _TRUNCATE);
		LOG("GameDebug: %s", g_status);
		return false;
	}

	const std::vector<ScriptDefines::Value> values = WantedValues();
	std::string text;
	std::string missing;

	if (!ScriptDefines::Rewrite(g_shipped, values.data(), static_cast<int>(values.size()), text, missing))
	{
		sprintf_s(g_status, "this game version has no %s, nothing is changed", missing.c_str());
		LOG("GameDebug: %s", g_status);
		return false;
	}

	if (text == g_written)
		return false;

	if (!WriteText(GeneratedPath(), text))
	{
		strncpy_s(g_status, "the debug defines could not be written", _TRUNCATE);
		LOG("GameDebug: %s", g_status);
		return false;
	}

	g_written = text;
	LOG("GameDebug: %d debug constant(s) on%s", static_cast<int>(values.size()),
		g_allowGameChanges ? "" : ", the ones that change the game held back");
	return true;
}

void Reload()
{
	if (!BattleDataReload::IsSupported())
	{
		strncpy_s(g_status, "applies the next time the game starts", _TRUNCATE);
		g_reloadWanted = false;
		return;
	}

	if (!BattleDataReload::CanRunNow())
	{
		strncpy_s(g_status, "applies when you leave the match", _TRUNCATE);
		return;
	}

	g_reloadWanted = false;

	if (!BattleDataReload::Run())
	{
		sprintf_s(g_status, "the battle scripts could not be reloaded: %s", BattleDataReload::StatusText());
		LOG("GameDebug: %s", g_status);
		return;
	}

	strncpy_s(g_status, "applied", _TRUNCATE);
	LOG("GameDebug: battle scripts reloaded with the debug options");
}

void FollowContext()
{
	if (++g_frame % kContextEveryFrames != 0)
		return;

	const bool allow = AllowGameChanges();

	if (allow == g_allowGameChanges)
		return;

	g_allowGameChanges = allow;
	g_reloadWanted = Regenerate() || g_reloadWanted;
}

}

bool GameDebug::IsEnabled()
{
	return g_modVals.gameDebugEnabled;
}

void GameDebug::Prepare()
{
	if (!IsEnabled())
		return;

	g_allowGameChanges = AllowGameChanges();
	Regenerate();
	g_served = !g_written.empty();

	if (!g_served)
		return;

	strncpy_s(g_status, "applied", _TRUNCATE);
	ScriptConsole::SetOpen(g_modVals.gameDebugScriptLog);
}

void GameDebug::Layer(FileIndex& into)
{
	if (!g_served)
		return;

	into.Add(FileIndex::Key(std::string(kServedKey)), GeneratedPath());
}

void GameDebug::OnFrame()
{
	if (!g_served)
		return;

	ScriptConsole::SetOpen(g_modVals.gameDebugScriptLog);
	FollowContext();

	if (g_reloadWanted)
		Reload();
}

int GameDebug::OptionCount()
{
	return kEntryCount;
}

const GameDebug::Option& GameDebug::OptionAt(int index)
{
	return kEntries[index].option;
}

void GameDebug::Save(int index)
{
	if (index < 0 || index >= kEntryCount)
		return;

	const Option& option = kEntries[index].option;
	Settings::SaveInt(kSection, option.key, *option.value ? 1 : 0);

	g_reloadWanted = Regenerate() || g_reloadWanted;
}

bool GameDebug::HoldsBackGameChanges()
{
	return !g_allowGameChanges;
}

const char* GameDebug::StatusText()
{
	return g_status;
}
