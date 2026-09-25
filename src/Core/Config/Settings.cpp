#include "Core/Config/Settings.h"

#include "Core/Profiler.h"
#include "Core/Config/default_ini.h"
#include "Core/Config/interfaces.h"
#include "Core/Config/keycodes.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/Post/PostOptions.h"
#include "D3D9/Post/UpscaleFilter.h"
#include "Game/Battle/KeyboardSeat.h"
#include "Game/Display/PotatoMode.h"
#include "Game/Replays/ReplayFiles.h"
#include "Game/Lobby/NameCensor.h"
#include "Game/Lobby/RoomNameCensor.h"
#include "Game/Subtitles/SubtitleTable.h"
#include "Game/Subtitles/SubtitleWatch.h"
#include "Game/Battle/ScreenShake.h"
#include "Training/Meter/FrameMeter.h"
#include "Training/StageColor.h"

#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>

namespace {

const char* const kIniFileName = "UNI2_IM.ini";

constexpr int kSettingsRevision = 4;

void FlushIniCache(const std::string& path)
{
	WritePrivateProfileStringA(nullptr, nullptr, nullptr, path.c_str());
}

int ClampRange(int value, int lowest, int highest)
{
	if (value < lowest)
		return lowest;

	if (value > highest)
		return highest;

	return value;
}

std::string ReadIniString(const char* section, const char* key, const char* defaultValue, const std::string& path)
{
	char buffer[512] = {};
	GetPrivateProfileStringA(section, key, defaultValue, buffer, sizeof(buffer), path.c_str());

	std::string value(buffer);
	value.erase(0, value.find_first_not_of(" \t"));
	const size_t last = value.find_last_not_of(" \t");
	if (last != std::string::npos)
		value.erase(last + 1);

	return value;
}

float ReadIniFloat(const char* section, const char* key, float defaultValue, const std::string& path)
{
	char defaultBuffer[64] = {};
	sprintf_s(defaultBuffer, "%f", defaultValue);

	const std::string value = ReadIniString(section, key, defaultBuffer, path);
	if (value.empty())
		return defaultValue;

	return static_cast<float>(atof(value.c_str()));
}

int ReadIniInt(const char* section, const char* key, int defaultValue, const std::string& path)
{
	return static_cast<int>(GetPrivateProfileIntA(section, key, defaultValue, path.c_str()));
}

bool WriteShippedIni(const std::string& path)
{

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "w") != 0 || file == nullptr)
		return false;

	const int count = static_cast<int>(sizeof(kDefaultIniLines) / sizeof(kDefaultIniLines[0]));

	for (int i = 0; i < count; ++i)
		fprintf(file, "%s\n", kDefaultIniLines[i]);

	fclose(file);
	return true;
}

std::string ReadFileText(const std::string& path)
{
	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr)
		return std::string();

	std::string text;
	char chunk[1024] = {};

	for (size_t read = fread(chunk, 1, sizeof(chunk), file); read > 0;
		read = fread(chunk, 1, sizeof(chunk), file))
	{
		text.append(chunk, read);
	}

	fclose(file);
	return text;
}

std::string TrimmedLine(const std::string& text, size_t begin, size_t end)
{
	while (begin < end && isspace(static_cast<unsigned char>(text[begin])))
		++begin;

	while (end > begin && isspace(static_cast<unsigned char>(text[end - 1])))
		--end;

	return text.substr(begin, end - begin);
}

bool HasSectionHeader(const std::string& text, const char* section)
{
	const std::string header = "[" + std::string(section) + "]";

	for (size_t begin = 0; begin < text.size();)
	{
		size_t end = text.find('\n', begin);
		if (end == std::string::npos)
			end = text.size();

		if (_stricmp(TrimmedLine(text, begin, end).c_str(), header.c_str()) == 0)
			return true;

		begin = end + 1;
	}

	return false;
}

int TrailingNewlines(const std::string& text)
{
	int count = 0;

	for (size_t i = text.size(); i > 0; --i)
	{
		const char character = text[i - 1];

		if (character == '\n')
		{
			++count;
			continue;
		}

		if (character == '\r')
			continue;

		break;
	}

	return count;
}

void AppendSectionHeader(const char* section, const std::string& path, const std::string& text)
{
	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "ab") != 0 || file == nullptr)
		return;

	const int wanted = text.empty() ? 0 : 2;

	for (int written = TrailingNewlines(text); written < wanted; ++written)
		fputs("\r\n", file);

	fprintf(file, "[%s]\r\n", section);
	fclose(file);
}

void EnsureSectionHeader(const char* section, const std::string& path)
{
	const std::string text = ReadFileText(path);
	if (HasSectionHeader(text, section))
		return;

	AppendSectionHeader(section, path, text);
}

const char* const kMissingMarker = "\x01";

bool KeyExists(const char* section, const char* key, const std::string& path)
{
	char buffer[8] = {};
	GetPrivateProfileStringA(section, key, kMissingMarker, buffer, sizeof(buffer), path.c_str());

	return strcmp(buffer, kMissingMarker) != 0;
}

bool AddMissingKey(const char* section, const char* key, const char* value, const std::string& path)
{
	if (KeyExists(section, key, path))
		return false;

	EnsureSectionHeader(section, path);

	if (!WritePrivateProfileStringA(section, key, value, path.c_str()))
	{
		LOG("Could not add [%s] %s to the ini (error %lu)", section, key, GetLastError());
		return false;
	}

	LOG("Added missing key to the ini: [%s] %s = %s", section, key, value);
	return true;
}

int CompleteIniFile(const std::string& path)
{
	int added = 0;

#define SETTING_STRING(member, section, key, defaultValue) \
	added += AddMissingKey(section, key, defaultValue, path) ? 1 : 0;
#define SETTING_FLOAT(member, section, key, defaultValue) \
	{ char buf[64] = {}; sprintf_s(buf, "%g", defaultValue); added += AddMissingKey(section, key, buf, path) ? 1 : 0; }
#define SETTING_INT(member, section, key, defaultValue) \
	{ char buf[64] = {}; sprintf_s(buf, "%d", defaultValue); added += AddMissingKey(section, key, buf, path) ? 1 : 0; }
#include "Core/Config/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT

	if (added > 0)
		FlushIniCache(path);

	return added;
}

void MigrateIni(int from, const std::string& path)
{
	if (from < 2)
	{
		WritePrivateProfileStringA("Netplay", "Diagnostics", "0", path.c_str());
		LOG("Settings: [Netplay] Diagnostics was turned off - it reaches into the netcode and is "
			"a diagnostic, not a feature");
	}

	if (from < 3)
	{
		WritePrivateProfileStringA("Netplay", "RoomRosterFix", "0", path.c_str());
		WritePrivateProfileStringA("Netplay", "RepublishPingLocation", "0", path.c_str());
		WritePrivateProfileStringA("Netplay", "SafeOnline", nullptr, path.c_str());
		WritePrivateProfileStringA("Netplay", "Diagnostics", nullptr, path.c_str());
		LOG("Settings: RoomRosterFix and RepublishPingLocation are off, they write into the game's own room state");
	}

	if (from < 4)
	{
		WritePrivateProfileStringA("Netplay", "NetLog", "0", path.c_str());
		WritePrivateProfileStringA("Netplay", "CaptureGgpoLog", "0", path.c_str());
		LOG("Settings: the network log is off unless you turn it on, it is for a report");
	}

	char revision[16] = {};
	sprintf_s(revision, "%d", kSettingsRevision);
	WritePrivateProfileStringA("Mod", "SettingsRevision", revision, path.c_str());
	FlushIniCache(path);

	LOG("Settings: brought the ini up from revision %d to %d", from, kSettingsRevision);
}

void WriteDefaultIni(const std::string& path)
{
	if (WriteShippedIni(path))
		return;

#define SETTING_STRING(member, section, key, defaultValue) \
	WritePrivateProfileStringA(section, key, defaultValue, path.c_str());
#define SETTING_FLOAT(member, section, key, defaultValue) \
	{ char buf[64] = {}; sprintf_s(buf, "%g", defaultValue); WritePrivateProfileStringA(section, key, buf, path.c_str()); }
#define SETTING_INT(member, section, key, defaultValue) \
	{ char buf[64] = {}; sprintf_s(buf, "%d", defaultValue); WritePrivateProfileStringA(section, key, buf, path.c_str()); }
#include "Core/Config/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT

	FlushIniCache(path);
}

}

std::string Settings::GetIniPath()
{
	return GetModRootPath(kIniFileName);
}

void Settings::SaveInt(const char* section, const char* key, int value)
{
	char buffer[32] = {};
	sprintf_s(buffer, "%d", value);

	const std::string path = GetIniPath();

	if (!WritePrivateProfileStringA(section, key, buffer, path.c_str()))
		LOG("Could not write %s/%s to the ini (error %lu)", section, key, GetLastError());

	FlushIniCache(path);
}

namespace {

void ParseStopList(const char* text, int* out)
{
	for (int i = 0; i < 4; ++i)
		out[i] = 0;

	int found = 0;

	while (*text != 0 && found < 4)
	{
		while (*text != 0 && (*text < '0' || *text > '9'))
			++text;

		if (*text == 0)
			break;

		int value = 0;
		while (*text >= '0' && *text <= '9' && value < 1000)
			value = value * 10 + (*text++ - '0');

		if (value > 0 && value < 100)
			out[found++] = value;
	}
}

}

void Settings::SaveString(const char* section, const char* key, const char* value)
{
	const std::string path = GetIniPath();

	if (!WritePrivateProfileStringA(section, key, value, path.c_str()))
		LOG("Could not write %s/%s to the ini (error %lu)", section, key, GetLastError());

	FlushIniCache(path);
}

void Settings::SaveFloat(const char* section, const char* key, float value)
{
	char buffer[32] = {};
	sprintf_s(buffer, "%g", value);

	const std::string path = GetIniPath();

	if (!WritePrivateProfileStringA(section, key, buffer, path.c_str()))
		LOG("Could not write %s/%s to the ini (error %lu)", section, key, GetLastError());

	FlushIniCache(path);
}

bool Settings::LoadSettingsFile()
{
	const std::string path = GetIniPath();
	const bool fresh = GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES;
	const int revision = fresh ? kSettingsRevision : ReadIniInt("Mod", "SettingsRevision", 0, path);

	if (fresh)
	{
		LOG("Settings file not found, writing defaults to %s", path.c_str());
		WriteDefaultIni(path);
	}
	else
	{
		const int added = CompleteIniFile(path);
		if (added > 0)
			LOG("Completed the ini with %d missing key%s", added, added == 1 ? "" : "s");

		if (revision < kSettingsRevision)
			MigrateIni(revision, path);
	}

#define SETTING_STRING(member, section, key, defaultValue) \
	g_settings.member = ReadIniString(section, key, defaultValue, path);
#define SETTING_FLOAT(member, section, key, defaultValue) \
	g_settings.member = ReadIniFloat(section, key, defaultValue, path);
#define SETTING_INT(member, section, key, defaultValue) \
	g_settings.member = ReadIniInt(section, key, defaultValue, path);
#include "Core/Config/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT

	return true;
}

void Settings::ApplySettings()
{
#define SETTING_STRING(member, section, key, defaultValue)
#define SETTING_FLOAT(member, section, key, defaultValue)
#define SETTING_INT(member, section, key, defaultValue)
#define SETTING_BOOL(member, section, key, defaultValue) g_modVals.member = g_settings.member != 0;
#define SETTING_CLAMP(member, section, key, defaultValue, low, high) \
	g_modVals.member = ClampRange(g_settings.member, low, high);
#define SETTING_RANGE(member, section, key, defaultValue, low, high) \
	g_modVals.member = g_settings.member < (low) || g_settings.member > (high) ? (defaultValue) \
		: g_settings.member;
#define SETTING_RANGE_FLOAT(member, section, key, defaultValue, low, high) \
	g_modVals.member = !(g_settings.member >= (low)) || g_settings.member > (high) ? (defaultValue) \
		: g_settings.member;
#define SETTING_COPY(member, section, key, defaultValue) g_modVals.member = g_settings.member;
#include "Core/Config/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT
#undef SETTING_BOOL
#undef SETTING_CLAMP
#undef SETTING_RANGE
#undef SETTING_RANGE_FLOAT
#undef SETTING_COPY

	g_modVals.toggleOverlayKey = GetVirtualKeyFromName(g_settings.toggleOverlayKey);
	g_modVals.toggleHitboxKey = GetVirtualKeyFromName(g_settings.toggleHitboxKey);
	g_modVals.toggleFrameMeterKey = GetVirtualKeyFromName(g_settings.toggleFrameMeterKey);
	g_modVals.hideHudKey = GetVirtualKeyFromName(g_settings.hideHudKey);
	g_modVals.restartGameKey = GetVirtualKeyFromName(g_settings.restartGameKey);
	g_modVals.freezeFrameKey = GetVirtualKeyFromName(g_settings.freezeFrameKey);
	g_modVals.stepForwardKey = GetVirtualKeyFromName(g_settings.stepForwardKey);
	g_modVals.nextPaletteKey = GetVirtualKeyFromName(g_settings.nextPaletteKey);
	g_modVals.prevPaletteKey = GetVirtualKeyFromName(g_settings.prevPaletteKey);
	g_modVals.functionKey = GetVirtualKeyFromName(g_settings.functionKey);

	g_modVals.freezeMode = g_settings.freezeMode == 1 ? 1 : 0;
	g_modVals.showHealthValues = g_settings.showHealthValues != 0 ? 1 : 0;
	g_modVals.hideBattleHud = g_settings.hideBattleHud != 0 ? 1 : 0;

	if (g_modVals.autoPauseMode < 0)
		g_modVals.autoPauseMode = 0;

	ParseStopList(g_settings.autoPauseComboStops.c_str(), g_modVals.autoPauseComboStops);
	ParseStopList(g_settings.autoPauseBlockStops.c_str(), g_modVals.autoPauseBlockStops);

	if (g_modVals.recordFrameCounterRva < 0)
		g_modVals.recordFrameCounterRva = 0;

	for (int i = 0; i < 32; ++i)
		g_modVals.paletteCompanion[i] = false;

	for (const char* text = g_settings.paletteCompanions.c_str(); *text != 0; )
	{
		while (*text != 0 && (*text < '0' || *text > '9'))
			++text;

		if (*text == 0)
			break;

		int value = 0;
		while (*text >= '0' && *text <= '9' && value < 1000)
			value = value * 10 + (*text++ - '0');

		if (value >= 0 && value < 32)
			g_modVals.paletteCompanion[value] = true;
	}

	g_modVals.showLegacyPalettes = g_settings.showlegacypalettes != 0;

	const bool flatStage = g_settings.stageFlatColour != 0 || g_settings.simpleStage != 0;

	StageColor::SetColor(static_cast<uint32_t>(g_settings.stageFlatColourValue));
	StageColor::SetEnabled(flatStage);

	if (g_modVals.keyboardSeat < KeyboardSeat::Seat_Default ||
		g_modVals.keyboardSeat > KeyboardSeat::Seat_P2)
	{
		g_modVals.keyboardSeat = KeyboardSeat::Seat_Default;
	}

	ReplayFiles::SetAutoExport(g_modVals.replayAutoExport);

	g_modVals.potatoHeight = PotatoMode::ClampHeight(g_settings.potatoHeight);

	g_modVals.sharpenMode = SharpenMode::Clamp(g_settings.sharpenMode);
	if (g_modVals.sharpenMode == SharpenMode::Kind_Off && g_modVals.sharpenStrength > 0)
		g_modVals.sharpenMode = SharpenMode::Kind_Cas;

	g_modVals.antiAliasing = AntiAlias::Clamp(g_settings.antiAliasing);

	g_modVals.upscaleFilter = UpscaleFilter::Clamp(g_settings.upscaleFilter);
	if (g_modVals.upscaleFilter == UpscaleFilter::Kind_Off && g_settings.sceneUpscale != 0)
		g_modVals.upscaleFilter = UpscaleFilter::Kind_Easu;

	if (g_modVals.presentWidth < 256 || g_modVals.presentHeight < 144 ||
		g_modVals.presentWidth > 7680 || g_modVals.presentHeight > 4320)
	{
		g_modVals.presentWidth = 0;
		g_modVals.presentHeight = 0;
	}

	g_modVals.simpleStage = flatStage;

	ScreenShake::SetIntensity(g_modVals.screenShake);

	if (g_modVals.potatoMode < 0 || g_modVals.potatoMode >= PotatoMode::Level_COUNT)
		g_modVals.potatoMode = PotatoMode::Level_Off;

	NameCensor::SetMask(g_settings.censorNameMask.c_str());
	NameCensor::SetCoversOwnName(g_modVals.censorOwnName);
	NameCensor::SetEnabled(g_modVals.censorNames);

	RoomNameCensor::SetEnabled(g_modVals.censorRoomNames);

	SubtitleTable::Choose(g_settings.subtitlePack.c_str());
	SubtitleWatch::SetHoldMs(g_modVals.subtitleHoldMs);
	SubtitleWatch::SetEnabled(g_modVals.subtitles);

	Profiler::SetEnabled(g_modVals.profilerEnabled);
	FrameMeter::SetTraceEnabled(g_modVals.meterTrace);

	LOG("Settings applied: overlay=%d hitbox=%d frameMeter=%d freeze=%d step=%d "
		"freezeMode=%d blockMouse=%d scale=%.2f",
		g_modVals.toggleOverlayKey, g_modVals.toggleHitboxKey, g_modVals.toggleFrameMeterKey,
		g_modVals.freezeFrameKey, g_modVals.stepForwardKey, g_modVals.freezeMode,
		g_modVals.blockGameMouse ? 1 : 0, static_cast<double>(g_modVals.uiScale));
}
