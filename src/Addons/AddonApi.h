#pragma once

#include <cstddef>
#include <cstdint>

#define UNI2_ADDON_API_VERSION 2u
#define UNI2_ADDON_INIT_EXPORT "UNI2_AddonInit"
#define UNI2_ADDON_SHUTDOWN_EXPORT "UNI2_AddonShutdown"

extern "C" {

enum Uni2AddonLogLevel
{
	UNI2_LOG_INFO = 0,
	UNI2_LOG_WARN = 1,
	UNI2_LOG_ERROR = 2,
	UNI2_LOG_DEBUG = 3
};

enum Uni2AddonToastKind
{
	UNI2_TOAST_INFO = 0,
	UNI2_TOAST_SUCCESS = 1,
	UNI2_TOAST_WARNING = 2,
	UNI2_TOAST_ERROR = 3
};

enum Uni2GameEnding
{
	UNI2_ENDING_DECIDED = 0,
	UNI2_ENDING_OPPONENT_LEFT = 1,
	UNI2_ENDING_CONNECTION_LOST = 2
};

struct Uni2GameResult
{
	int ending;
	int winner;
	int localSide;
	uint64_t steamId[2];
	int chara[2];
	const uint8_t* record;
	size_t recordSize;
};

struct Uni2AddonHost
{
	uint32_t apiVersion;
	const char* imVersion;
	const char* gameDir;
	const char* modRootDir;

	void (__stdcall* log)(int level, const char* message);
	void* (__stdcall* getD3D9Device)();
	void* (__stdcall* getGameWindow)();
	void* (__stdcall* getImGuiContext)();

	uintptr_t (__stdcall* rvaToAddress)(uintptr_t rva);
	uintptr_t (__stdcall* signatureAddress)(uintptr_t rva);
	bool (__stdcall* readMemory)(void* dst, uintptr_t address, size_t size);
	bool (__stdcall* writeMemory)(uintptr_t address, const void* src, size_t size);

	void (__stdcall* registerFrameCallback)(void (__stdcall* cb)(void*), void* user);
	void (__stdcall* registerOverlayCallback)(void (__stdcall* cb)(void*), void* user);
	void (__stdcall* registerHudCallback)(void (__stdcall* cb)(void*), void* user);
	void (__stdcall* registerWndProcCallback)(
		bool (__stdcall* cb)(void* hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam, void* user),
		void* user);
	void (__stdcall* registerDeviceCallbacks)(
		void (__stdcall* onLost)(void*),
		void (__stdcall* onReset)(void*),
		void* user);

	void (__stdcall* showToast)(int kind, const char* title, const char* text);
	void (__stdcall* notify)(const char* text);
	bool (__stdcall* addMainMenuButton)(
		const char* id,
		const char* label,
		const char* summary,
		void (__stdcall* onToggle)(void*),
		bool (__stdcall* isOpen)(void*),
		void* user);

	void (__stdcall* registerRenderCallback)(void (__stdcall* cb)(void*), void* user);
	void (__stdcall* registerGameEvents)(
		void (__stdcall* onStarted)(void*),
		void (__stdcall* onEnded)(const Uni2GameResult* result, void*),
		void* user);
	void (__stdcall* setOverlayBlocksInput)(bool block);

	bool (__stdcall* createHook)(void* target, void* detour, void** original, const char* label);
	bool (__stdcall* createApiHook)(const char* moduleName, const char* functionName, void* detour, void** original);
	bool (__stdcall* setHookEnabled)(void* target, bool enabled);

	int (__stdcall* getScene)();
	int (__stdcall* getSceneFrames)();
	bool (__stdcall* isInMatch)();
	bool (__stdcall* isOnline)();
	bool (__stdcall* isSpectating)();
	int (__stdcall* getLocalSide)();
	int (__stdcall* getLoadedCharacter)(int side);
	const char* (__stdcall* getCharacterName)(int charaId);

	uint64_t (__stdcall* getLocalSteamId)();
	void (__stdcall* setRichPresenceNote)(const char* note);
	bool (__stdcall* getReplayFolder)(char* outBuf, size_t outSize);
	bool (__stdcall* saveReplayRecord)(
		const uint8_t* record,
		size_t size,
		const char* note,
		const char* name0,
		const char* name1,
		char* outPath,
		size_t outPathSize);

	bool (__stdcall* configGetString)(
		const char* section,
		const char* key,
		const char* fallback,
		char* outBuf,
		size_t outSize);
	int (__stdcall* configGetInt)(const char* section, const char* key, int fallback);
	void (__stdcall* configSetString)(const char* section, const char* key, const char* value);
	void (__stdcall* configSetInt)(const char* section, const char* key, int value);
};

typedef bool (__stdcall* Uni2AddonInitFn)(const Uni2AddonHost* host, const char* addonDir);
typedef void (__stdcall* Uni2AddonShutdownFn)();

}
