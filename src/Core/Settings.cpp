#include "Core/Settings.h"

#include "Core/Profiler.h"
#include "Core/default_ini.h"
#include "Core/interfaces.h"
#include "Core/keycodes.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Training/FrameMeter.h"
#include "Training/StageColor.h"

#include <Windows.h>
#include <cstdio>
#include <cstdlib>

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

	g_modVals.freezeMode = g_settings.freezeMode == 1 ? 1 : 0;
	g_modVals.blockGameMouse = g_settings.blockGameMouse != 0;

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

	StageColor::SetColor(static_cast<uint32_t>(g_settings.stageFlatColourValue));
	StageColor::SetEnabled(g_settings.stageFlatColour != 0);

	g_modVals.timerResolution = g_settings.timerResolution != 0;
	g_modVals.powerThrottlingOptOut = g_settings.powerThrottlingOptOut != 0;
	g_modVals.displayTuning = g_settings.displayTuning != 0;

	g_modVals.fullscreenRefreshHz = g_settings.fullscreenRefreshHz;
	if (g_modVals.fullscreenRefreshHz < 0 || g_modVals.fullscreenRefreshHz > 1000)
		g_modVals.fullscreenRefreshHz = 0;

	g_modVals.extraBackBuffer = g_settings.extraBackBuffer != 0;
	g_modVals.pumpWait = g_settings.pumpWait != 0;
	g_modVals.pumpWaitAllInput = g_settings.pumpWaitAllInput != 0;

	g_modVals.internalResolutionPercent = g_settings.internalResolutionPercent;
	if (g_modVals.internalResolutionPercent < 25 || g_modVals.internalResolutionPercent > 400)
		g_modVals.internalResolutionPercent = 100;

	g_modVals.internalResolutionBudgetMb = g_settings.internalResolutionBudgetMb;
	if (g_modVals.internalResolutionBudgetMb < 16 || g_modVals.internalResolutionBudgetMb > 1024)
		g_modVals.internalResolutionBudgetMb = 192;

	g_modVals.disableBackBufferAa = g_settings.disableBackBufferAa != 0;
	g_modVals.simpleStage = g_settings.simpleStage != 0;
	g_modVals.potatoMode = g_settings.potatoMode != 0;

	g_modVals.uiScale = g_settings.uiScale;
	if (g_modVals.uiScale < 0.5f || g_modVals.uiScale > 4.0f)
		g_modVals.uiScale = 1.0f;

	g_modVals.notifications = g_settings.notifications != 0;

	g_modVals.memoryDebugEnabled = g_settings.memoryDebugEnabled != 0;
	g_modVals.profilerEnabled = g_settings.profilerEnabled != 0;
	g_modVals.meterTrace = g_settings.meterTrace != 0;

	Profiler::SetEnabled(g_modVals.profilerEnabled);
	FrameMeter::SetTraceEnabled(g_modVals.meterTrace);

	LOG("Settings applied: overlay=%d hitbox=%d frameMeter=%d freeze=%d step=%d back=%d "
		"freezeMode=%d blockMouse=%d scale=%.2f",
		g_modVals.toggleOverlayKey, g_modVals.toggleHitboxKey, g_modVals.toggleFrameMeterKey,
		g_modVals.freezeFrameKey, g_modVals.stepForwardKey, g_modVals.freezeMode,
		g_modVals.blockGameMouse ? 1 : 0, g_modVals.uiScale);
}
