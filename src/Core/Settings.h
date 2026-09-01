#pragma once

#include <string>

struct SettingsIni
{
#define SETTING_STRING(member, section, key, defaultValue) std::string member;
#define SETTING_FLOAT(member, section, key, defaultValue) float member;
#define SETTING_INT(member, section, key, defaultValue) int member;
#include "Core/settings.def"
#undef SETTING_STRING
#undef SETTING_FLOAT
#undef SETTING_INT
};

struct ModValues
{
	int toggleOverlayKey;
	int toggleHitboxKey;
	int toggleFrameMeterKey;
	int freezeFrameKey;
	int stepForwardKey;
	int nextPaletteKey;
	int prevPaletteKey;
	int functionKey;

	bool checkForUpdates;

	int freezeMode;
	bool blockGameMouse;
	bool drawWhilePaused;

	int autoPauseMode;
	int autoPauseComboStops[4];
	int autoPauseBlockStops[4];
	int resumeDelayFrames;
	int recordFrameCounterRva;

	int stepRepeatDelayMs;
	int stepRepeatIntervalMs;

	bool frameMeterAuto;
	int frameMeterX;
	int frameMeterY;
	float frameMeterScale;
	bool frameMeterCounts;
	bool frameMeterTotals;
	bool frameMeterAttributes;
	int frameMeterOpacity;
	bool frameMeterDrag;

	bool paletteCompanion[32];

	bool showOnlinePalettes;
	bool paletteOwnersFromDraws;
	bool paletteIdentifyByColours;
	bool paletteOutOfMatch;
	bool paletteEffectRows;
	bool showLegacyPalettes;
	bool paletteGroupByPart;
	bool paletteFlashEntry;
	bool paletteFilterJunk;

	bool timerResolution;
	bool powerThrottlingOptOut;
	bool displayTuning;
	int fullscreenRefreshHz;
	bool extraBackBuffer;
	bool pumpWait;
	bool pumpWaitAllInput;

	int wineSafeMode;

	int keyboardSeat;
	bool keyboardSeatRouteSides;

	bool replayAutoExport;

	bool onlineSafety;
	bool roomRosterFix;
	bool republishPingLocation;
	bool netplayDiagnostics;
	bool sharePalettes;
	bool unloadPatchOnline;

	int presentWidth;
	int presentHeight;
	int potatoHeight;
	int supersample;
	int sharpenStrength;
	int sharpenMode;
	int upscaleFilter;
	int antiAliasing;
	bool bloomEnabled;
	int bloomIntensity;
	int bloomThreshold;
	bool lookEnabled;
	int lookBrightness;
	int lookContrast;
	int lookGamma;
	int lookSaturation;
	int lookVibrance;
	int lookTemperature;
	int lookVignette;
	int lookScanlines;
	bool lookDither;
	int sceneScalePercent;
	bool sceneDrawScale;
	bool scenePinProjection;
	bool sceneReferenceScale;
	bool sceneReferenceLiterals;
	bool disableBackBufferAa;
	bool disableCharacterFilter;
	bool simpleStage;
	int screenShake;
	bool keepMenuMusic;
	int potatoMode;

	float uiScale;
	float fontSize;
	bool dpiAware;
	bool notifications;

	bool memoryDebugEnabled;
	bool profilerEnabled;
	bool meterTrace;
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
