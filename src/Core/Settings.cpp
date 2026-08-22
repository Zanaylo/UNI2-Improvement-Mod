#include "Core/Settings.h"

#include "Core/Profiler.h"
#include "Core/default_ini.h"
#include "Core/interfaces.h"
#include "Core/keycodes.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/PotatoMode.h"
#include "Training/FrameMeter.h"
#include "Training/StageColor.h"

#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>

namespace {

const char* const kIniFileName = "UNI2_IM.ini";

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

// The generated ini would carry every setting and not one word about what any of them does. The
// shipped file is the documentation, so a fresh install gets that; the key-by-key writer stays as
// the fallback for when the file cannot be written whole.
bool WriteShippedIni(const std::string& path)
{
	// Text mode, so the newlines come out as an ini file on Windows is expected to have them.
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
#include "Core/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT

	return added;
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
#include "Core/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT
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

	if (!WritePrivateProfileStringA(section, key, buffer, GetIniPath().c_str()))
		LOG("Could not write %s/%s to the ini (error %lu)", section, key, GetLastError());
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
	if (!WritePrivateProfileStringA(section, key, value, GetIniPath().c_str()))
		LOG("Could not write %s/%s to the ini (error %lu)", section, key, GetLastError());
}

void Settings::SaveFloat(const char* section, const char* key, float value)
{
	char buffer[32] = {};
	sprintf_s(buffer, "%g", value);

	if (!WritePrivateProfileStringA(section, key, buffer, GetIniPath().c_str()))
		LOG("Could not write %s/%s to the ini (error %lu)", section, key, GetLastError());
}

bool Settings::LoadSettingsFile()
{
	const std::string path = GetIniPath();

	if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		LOG("Settings file not found, writing defaults to %s", path.c_str());
		WriteDefaultIni(path);
	}
	else
	{
		const int added = CompleteIniFile(path);
		if (added > 0)
			LOG("Completed the ini with %d missing key%s", added, added == 1 ? "" : "s");
	}

#define SETTING_STRING(member, section, key, defaultValue) \
	g_settings.member = ReadIniString(section, key, defaultValue, path);
#define SETTING_FLOAT(member, section, key, defaultValue) \
	g_settings.member = ReadIniFloat(section, key, defaultValue, path);
#define SETTING_INT(member, section, key, defaultValue) \
	g_settings.member = ReadIniInt(section, key, defaultValue, path);
#include "Core/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT

	return true;
}

void Settings::ApplySettings()
{
	g_modVals.toggleOverlayKey = GetVirtualKeyFromName(g_settings.toggleOverlayKey);
	g_modVals.toggleHitboxKey = GetVirtualKeyFromName(g_settings.toggleHitboxKey);
	g_modVals.toggleFrameMeterKey = GetVirtualKeyFromName(g_settings.toggleFrameMeterKey);
	g_modVals.freezeFrameKey = GetVirtualKeyFromName(g_settings.freezeFrameKey);
	g_modVals.stepForwardKey = GetVirtualKeyFromName(g_settings.stepForwardKey);
	g_modVals.nextPaletteKey = GetVirtualKeyFromName(g_settings.nextPaletteKey);
	g_modVals.prevPaletteKey = GetVirtualKeyFromName(g_settings.prevPaletteKey);
	g_modVals.functionKey = GetVirtualKeyFromName(g_settings.functionKey);

	g_modVals.checkForUpdates = g_settings.checkForUpdates != 0;

	g_modVals.freezeMode = g_settings.freezeMode == 1 ? 1 : 0;
	g_modVals.blockGameMouse = g_settings.blockGameMouse != 0;
	g_modVals.drawWhilePaused = g_settings.drawWhilePaused != 0;

	g_modVals.autoPauseMode = g_settings.autoPauseMode;
	if (g_modVals.autoPauseMode < 0)
		g_modVals.autoPauseMode = 0;

	ParseStopList(g_settings.autoPauseComboStops.c_str(), g_modVals.autoPauseComboStops);
	ParseStopList(g_settings.autoPauseBlockStops.c_str(), g_modVals.autoPauseBlockStops);

	g_modVals.resumeDelayFrames = g_settings.resumeDelayFrames;
	if (g_modVals.resumeDelayFrames < 5 || g_modVals.resumeDelayFrames > 600)
		g_modVals.resumeDelayFrames = 60;

	g_modVals.recordFrameCounterRva = g_settings.recordFrameCounterRva;
	if (g_modVals.recordFrameCounterRva < 0)
		g_modVals.recordFrameCounterRva = 0;

	g_modVals.stepRepeatDelayMs = g_settings.stepRepeatDelayMs;
	if (g_modVals.stepRepeatDelayMs < 0 || g_modVals.stepRepeatDelayMs > 3000)
		g_modVals.stepRepeatDelayMs = 250;

	g_modVals.stepRepeatIntervalMs = g_settings.stepRepeatIntervalMs;
	if (g_modVals.stepRepeatIntervalMs < 16 || g_modVals.stepRepeatIntervalMs > 2000)
		g_modVals.stepRepeatIntervalMs = 90;

	g_modVals.frameMeterAuto = g_settings.frameMeterAuto != 0;
	g_modVals.frameMeterX = g_settings.frameMeterX;
	g_modVals.frameMeterY = g_settings.frameMeterY;
	g_modVals.frameMeterScale = g_settings.frameMeterScale;
	if (g_modVals.frameMeterScale < 0.5f || g_modVals.frameMeterScale > 4.0f)
		g_modVals.frameMeterScale = 1.5f;

	g_modVals.frameMeterCounts = g_settings.frameMeterCounts != 0;
	g_modVals.frameMeterTotals = g_settings.frameMeterTotals != 0;
	g_modVals.frameMeterAttributes = g_settings.frameMeterAttributes != 0;
	g_modVals.frameMeterOpacity = g_settings.frameMeterOpacity;
	if (g_modVals.frameMeterOpacity < 10 || g_modVals.frameMeterOpacity > 100)
		g_modVals.frameMeterOpacity = 100;

	g_modVals.frameMeterDrag = g_settings.frameMeterDrag != 0;

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

	g_modVals.showOnlinePalettes = g_settings.showOnlinePalettes != 0;
	g_modVals.paletteOwnersFromDraws = g_settings.paletteOwnersFromDraws != 0;
	g_modVals.paletteIdentifyByColours = g_settings.paletteIdentifyByColours != 0;
	g_modVals.paletteOutOfMatch = g_settings.paletteOutOfMatch != 0;
	g_modVals.paletteEffectRows = g_settings.paletteEffectRows != 0;
	g_modVals.showLegacyPalettes = g_settings.showlegacypalettes != 0;
	g_modVals.paletteGroupByPart = g_settings.paletteGroupByPart != 0;
	g_modVals.paletteFlashEntry = g_settings.paletteFlashEntry != 0;
	g_modVals.paletteFilterJunk = g_settings.paletteFilterJunk != 0;

	// [Video] FlatStage and [Graphics] SimpleStage are one switch with two names. Left to drift they
	// disagree, and FlatStage = 1 with the tab's box unticked is a game with no stage and nothing
	// saying why.
	const bool flatStage = g_settings.stageFlatColour != 0 || g_settings.simpleStage != 0;

	StageColor::SetColor(static_cast<uint32_t>(g_settings.stageFlatColourValue));
	StageColor::SetEnabled(flatStage);

	g_modVals.timerResolution = g_settings.timerResolution != 0;
	g_modVals.powerThrottlingOptOut = g_settings.powerThrottlingOptOut != 0;
	g_modVals.displayTuning = g_settings.displayTuning != 0;

	g_modVals.fullscreenRefreshHz = g_settings.fullscreenRefreshHz;
	if (g_modVals.fullscreenRefreshHz < 0 || g_modVals.fullscreenRefreshHz > 1000)
		g_modVals.fullscreenRefreshHz = 0;

	g_modVals.extraBackBuffer = g_settings.extraBackBuffer != 0;
	g_modVals.pumpWait = g_settings.pumpWait != 0;
	g_modVals.pumpWaitAllInput = g_settings.pumpWaitAllInput != 0;
	g_modVals.wineSafeMode = g_settings.wineSafeMode;



	g_modVals.presentWidth = g_settings.presentWidth;
	g_modVals.presentHeight = g_settings.presentHeight;
	g_modVals.potatoHeight = PotatoMode::ClampHeight(g_settings.potatoHeight);

	if (g_modVals.presentWidth < 256 || g_modVals.presentHeight < 144 ||
		g_modVals.presentWidth > 7680 || g_modVals.presentHeight > 4320)
	{
		g_modVals.presentWidth = 0;
		g_modVals.presentHeight = 0;
	}

	g_modVals.disableBackBufferAa = g_settings.disableBackBufferAa != 0;
	g_modVals.disableCharacterFilter = g_settings.disableCharacterFilter != 0;

	g_modVals.simpleStage = flatStage;

	g_modVals.potatoMode = g_settings.potatoMode;
	if (g_modVals.potatoMode < 0 || g_modVals.potatoMode >= PotatoMode::Level_COUNT)
		g_modVals.potatoMode = PotatoMode::Level_Off;

	g_modVals.uiScale = g_settings.uiScale;
	if (g_modVals.uiScale < 0.5f || g_modVals.uiScale > 4.0f)
		g_modVals.uiScale = 1.0f;

	g_modVals.notifications = g_settings.notifications != 0;

	g_modVals.memoryDebugEnabled = g_settings.memoryDebugEnabled != 0;
	g_modVals.profilerEnabled = g_settings.profilerEnabled != 0;
	g_modVals.meterTrace = g_settings.meterTrace != 0;

	Profiler::SetEnabled(g_modVals.profilerEnabled);
	FrameMeter::SetTraceEnabled(g_modVals.meterTrace);

	LOG("Settings applied: overlay=%d hitbox=%d frameMeter=%d freeze=%d step=%d "
		"freezeMode=%d blockMouse=%d scale=%.2f",
		g_modVals.toggleOverlayKey, g_modVals.toggleHitboxKey, g_modVals.toggleFrameMeterKey,
		g_modVals.freezeFrameKey, g_modVals.stepForwardKey, g_modVals.freezeMode,
		g_modVals.blockGameMouse ? 1 : 0, static_cast<double>(g_modVals.uiScale));
}
