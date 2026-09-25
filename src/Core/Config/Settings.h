#pragma once

#include <string>

struct SettingsIni
{
#define SETTING_STRING(member, section, key, defaultValue) std::string member;
#define SETTING_FLOAT(member, section, key, defaultValue) float member;
#define SETTING_INT(member, section, key, defaultValue) int member;
#include "Core/Config/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT
};

struct ModValues
{
#define SETTING_STRING(member, section, key, defaultValue)
#define SETTING_FLOAT(member, section, key, defaultValue)
#define SETTING_INT(member, section, key, defaultValue)
#define SETTING_BOOL(member, section, key, defaultValue) bool member;
#define SETTING_CLAMP(member, section, key, defaultValue, low, high) int member;
#define SETTING_RANGE(member, section, key, defaultValue, low, high) int member;
#define SETTING_RANGE_FLOAT(member, section, key, defaultValue, low, high) float member;
#define SETTING_COPY(member, section, key, defaultValue) int member;
#include "Core/Config/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT
#undef SETTING_BOOL
#undef SETTING_CLAMP
#undef SETTING_RANGE
#undef SETTING_RANGE_FLOAT
#undef SETTING_COPY

	int toggleOverlayKey;
	int toggleHitboxKey;
	int toggleFrameMeterKey;
	int freezeFrameKey;
	int stepForwardKey;
	int nextPaletteKey;
	int prevPaletteKey;
	int hideHudKey;
	int restartGameKey;
	int functionKey;

	int freezeMode;

	int showHealthValues;
	int hideBattleHud;

	int autoPauseComboStops[4];
	int autoPauseBlockStops[4];

	bool paletteCompanion[32];

	bool showLegacyPalettes;

	int potatoHeight;
	int sharpenMode;
	int upscaleFilter;
	int antiAliasing;
	bool simpleStage;
};

namespace Settings
{
	bool LoadSettingsFile();
	void ApplySettings();
	std::string GetIniPath();

	void SaveInt(const char* section, const char* key, int value);
	void SaveString(const char* section, const char* key, const char* value);
	void SaveFloat(const char* section, const char* key, float value);
}
