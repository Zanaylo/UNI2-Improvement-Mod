#pragma once

#include "Network/GameResults.h"
#include "Web/HttpRequest.h"

#include <lua.hpp>

#include <string>
#include <vector>

namespace LuaHost
{
	struct AddonWindow
	{
		std::string id;
		std::string title;
		std::string summary;
		float width = 420.0f;
		float height = 360.0f;
		float minWidth = 280.0f;
		float minHeight = 200.0f;
		float maxWidth = 0.0f;
		float maxHeight = 0.0f;
		bool open = false;
		int drawRef = LUA_NOREF;
	};

	struct Instance
	{
		std::string id;
		std::string name;
		std::string folder;
		lua_State* state = nullptr;
		std::vector<int> frameCallbacks;
		std::vector<int> hudCallbacks;
		std::vector<int> gameStartedCallbacks;
		std::vector<int> gameEndedCallbacks;
		std::vector<int> shutdownCallbacks;
		std::vector<AddonWindow> windows;
	};

	bool LoadAddon(const std::string& id, const std::string& name, const std::string& folder,
		const std::string& entryScript, std::string& outError);

	void OnFrame();
	void OnHud();
	void OnOverlay();
	void OnGameStarted();
	void OnGameEnded(const GameResults::Report& report);
	void UnloadAll();
	void Shutdown();

	bool AnyWindowOpen();
	bool WantsHud();
	void DrawLauncherCards();

	Instance* InstanceOf(lua_State* state);
	void EnqueueHttp(Instance* owner, HttpRequest::Request request, int callbackRef);

	void RegisterCoreBindings(lua_State* state);
	void RegisterUiBindings(lua_State* state);
}
