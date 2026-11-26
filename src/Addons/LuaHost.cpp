#include "Addons/LuaHost.h"

#include "Core/logger.h"
#include "Game/Engine/SceneWatch.h"
#include "Overlay/Widgets/Cards.h"
#include "Overlay/Widgets/UiScale.h"
#include "Overlay/Widgets/UiText.h"

#include <imgui.h>

#include <cfloat>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace {

constexpr char kInstanceKey[] = "uni2_addon_instance";

struct HttpJob
{
	LuaHost::Instance* owner = nullptr;
	HttpRequest::Request request;
	int callbackRef = LUA_NOREF;
};

struct HttpResult
{
	LuaHost::Instance* owner = nullptr;
	int callbackRef = LUA_NOREF;
	bool sent = false;
	HttpRequest::Response response;
	std::string error;
};

std::vector<std::unique_ptr<LuaHost::Instance>> g_instances;
std::recursive_mutex g_vmMutex;
uint32_t g_lastScene = 0;
bool g_hasLastScene = false;

std::mutex g_httpMutex;
std::condition_variable g_httpWake;
std::deque<HttpJob> g_httpPending;
std::deque<HttpResult> g_httpCompleted;
bool g_httpStarted = false;
bool g_httpStop = false;

void HttpWorker()
{
	for (;;)
	{
		HttpJob job;

		{
			std::unique_lock<std::mutex> lock(g_httpMutex);
			g_httpWake.wait(lock, [] { return g_httpStop || !g_httpPending.empty(); });

			if (g_httpStop)
				return;

			job = std::move(g_httpPending.front());
			g_httpPending.pop_front();
		}

		HttpResult result;
		result.owner = job.owner;
		result.callbackRef = job.callbackRef;
		result.sent = HttpRequest::Send(job.request, result.response, result.error);

		std::lock_guard<std::mutex> lock(g_httpMutex);
		g_httpCompleted.push_back(std::move(result));
	}
}

void EnsureHttpWorker()
{
	if (g_httpStarted)
		return;

	g_httpStarted = true;
	g_httpStop = false;
	std::thread(HttpWorker).detach();
}

bool IsLiveInstance(LuaHost::Instance* target)
{
	for (const std::unique_ptr<LuaHost::Instance>& instance : g_instances)
	{
		if (instance.get() == target)
			return true;
	}

	return false;
}

void CallNoArgs(LuaHost::Instance& instance, int ref, const char* context)
{
	if (instance.state == nullptr || ref == LUA_NOREF)
		return;

	lua_rawgeti(instance.state, LUA_REGISTRYINDEX, ref);

	if (lua_pcall(instance.state, 0, 0, 0) != LUA_OK)
	{
		const char* const message = lua_tostring(instance.state, -1);
		LOG_ERROR("Addon %s (%s): %s", instance.id.c_str(), context, message != nullptr ? message : "error");
		lua_pop(instance.state, 1);
	}
}

void CallOneInt(LuaHost::Instance& instance, int ref, lua_Integer arg, const char* context)
{
	if (instance.state == nullptr || ref == LUA_NOREF)
		return;

	lua_rawgeti(instance.state, LUA_REGISTRYINDEX, ref);
	lua_pushinteger(instance.state, arg);

	if (lua_pcall(instance.state, 1, 0, 0) != LUA_OK)
	{
		const char* const message = lua_tostring(instance.state, -1);
		LOG_ERROR("Addon %s (%s): %s", instance.id.c_str(), context, message != nullptr ? message : "error");
		lua_pop(instance.state, 1);
	}
}

void CallTwoInts(LuaHost::Instance& instance, int ref, lua_Integer first, lua_Integer second, const char* context)
{
	if (instance.state == nullptr || ref == LUA_NOREF)
		return;

	lua_rawgeti(instance.state, LUA_REGISTRYINDEX, ref);
	lua_pushinteger(instance.state, first);
	lua_pushinteger(instance.state, second);

	if (lua_pcall(instance.state, 2, 0, 0) != LUA_OK)
	{
		const char* const message = lua_tostring(instance.state, -1);
		LOG_ERROR("Addon %s (%s): %s", instance.id.c_str(), context, message != nullptr ? message : "error");
		lua_pop(instance.state, 1);
	}
}

void DrainHttpResults()
{
	std::deque<HttpResult> ready;

	{
		std::lock_guard<std::mutex> lock(g_httpMutex);

		if (g_httpCompleted.empty())
			return;

		ready.swap(g_httpCompleted);
	}

	for (HttpResult& result : ready)
	{
		if (!IsLiveInstance(result.owner) || result.owner->state == nullptr)
			continue;

		lua_State* const L = result.owner->state;
		lua_rawgeti(L, LUA_REGISTRYINDEX, result.callbackRef);
		luaL_unref(L, LUA_REGISTRYINDEX, result.callbackRef);

		lua_newtable(L);
		lua_pushboolean(L, result.sent ? 1 : 0);
		lua_setfield(L, -2, "sent");
		lua_pushinteger(L, result.response.status);
		lua_setfield(L, -2, "status");
		lua_pushlstring(L, result.response.body.data(), result.response.body.size());
		lua_setfield(L, -2, "body");
		lua_pushstring(L, result.error.c_str());
		lua_setfield(L, -2, "error");

		if (lua_pcall(L, 1, 0, 0) != LUA_OK)
		{
			const char* const message = lua_tostring(L, -1);
			LOG_ERROR("Addon %s (http): %s", result.owner->id.c_str(), message != nullptr ? message : "error");
			lua_pop(L, 1);
		}
	}
}

void ConfigurePackagePath(lua_State* L, const std::string& folder)
{
	lua_getglobal(L, "package");

	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return;
	}

	const std::string path = folder + "\\?.lua;" + folder + "\\?\\init.lua";
	lua_pushstring(L, path.c_str());
	lua_setfield(L, -2, "path");

	lua_pushstring(L, "");
	lua_setfield(L, -2, "cpath");

	lua_pop(L, 1);
}

const char* EndingString(GameResults::Ending ending)
{
	switch (ending)
	{
	case GameResults::Ending::Decided:
		return "decided";
	case GameResults::Ending::OpponentLeft:
		return "opponent_left";
	default:
		return "connection_lost";
	}
}

std::string DecimalId(uint64_t value)
{
	char text[32] = {};
	sprintf_s(text, "%llu", static_cast<unsigned long long>(value));
	return text;
}

}

LuaHost::Instance* LuaHost::InstanceOf(lua_State* state)
{
	lua_getfield(state, LUA_REGISTRYINDEX, kInstanceKey);
	auto* const instance = static_cast<Instance*>(lua_touserdata(state, -1));
	lua_pop(state, 1);
	return instance;
}

void LuaHost::EnqueueHttp(Instance* owner, HttpRequest::Request request, int callbackRef)
{
	if (owner == nullptr)
		return;

	std::lock_guard<std::mutex> lock(g_httpMutex);
	EnsureHttpWorker();
	g_httpPending.push_back({ owner, std::move(request), callbackRef });
	g_httpWake.notify_one();
}

bool LuaHost::LoadAddon(const std::string& id, const std::string& name, const std::string& folder,
	const std::string& entryScript, std::string& outError)
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);
	lua_State* const L = luaL_newstate();

	if (L == nullptr)
	{
		outError = "could not create Lua state";
		return false;
	}

	luaL_openlibs(L);
	ConfigurePackagePath(L, folder);

	auto instance = std::make_unique<Instance>();
	instance->id = id;
	instance->name = name;
	instance->folder = folder;
	instance->state = L;

	lua_pushlightuserdata(L, instance.get());
	lua_setfield(L, LUA_REGISTRYINDEX, kInstanceKey);

	RegisterCoreBindings(L);
	RegisterUiBindings(L);

	const std::string fullPath = folder + "\\" + entryScript;

	if (luaL_loadfile(L, fullPath.c_str()) != LUA_OK)
	{
		const char* const message = lua_tostring(L, -1);
		outError = message != nullptr ? message : "failed to load script";
		lua_close(L);
		return false;
	}

	if (lua_pcall(L, 0, 0, 0) != LUA_OK)
	{
		const char* const message = lua_tostring(L, -1);
		outError = message != nullptr ? message : "failed to run script";
		lua_close(L);
		return false;
	}

	g_instances.push_back(std::move(instance));
	return true;
}

void LuaHost::OnFrame()
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);
	DrainHttpResults();

	const uint32_t scene = SceneWatch::Current();
	const bool sceneChanged = g_hasLastScene && scene != g_lastScene;
	const uint32_t previousScene = g_lastScene;
	g_lastScene = scene;
	g_hasLastScene = true;

	for (std::unique_ptr<Instance>& instance : g_instances)
	{
		if (sceneChanged)
		{
			for (int ref : instance->sceneChangedCallbacks)
				CallTwoInts(*instance, ref, static_cast<lua_Integer>(previousScene), static_cast<lua_Integer>(scene), "scene_changed");
		}

		for (int ref : instance->frameCallbacks)
			CallNoArgs(*instance, ref, "frame");
	}
}

void LuaHost::OnHud()
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);

	for (std::unique_ptr<Instance>& instance : g_instances)
	{
		for (int ref : instance->hudCallbacks)
			CallNoArgs(*instance, ref, "hud");
	}
}

void LuaHost::OnOverlay()
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float capW = display.x > 64.0f ? display.x * 0.90f : 1280.0f;
	const float capH = display.y > 64.0f ? display.y * 0.90f : 720.0f;

	for (std::unique_ptr<Instance>& instance : g_instances)
	{
		for (AddonWindow& window : instance->windows)
		{
			if (!window.open)
				continue;

			const float rawMaxW = window.maxWidth > 0.0f ? Ui::Scaled(window.maxWidth) : capW;
			const float rawMaxH = window.maxHeight > 0.0f ? Ui::Scaled(window.maxHeight) : capH;
			const float maxW = rawMaxW < capW ? rawMaxW : capW;
			const float maxH = rawMaxH < capH ? rawMaxH : capH;

			const float rawMinW = Ui::Scaled(window.minWidth);
			const float rawMinH = Ui::Scaled(window.minHeight);
			const float minW = rawMinW < maxW ? rawMinW : maxW;
			const float minH = rawMinH < maxH ? rawMinH : maxH;

			float initW = Ui::Scaled(window.width);
			float initH = Ui::Scaled(window.height);
			if (initW < minW) initW = minW;
			if (initW > maxW) initW = maxW;
			if (initH < minH) initH = minH;
			if (initH > maxH) initH = maxH;

			ImGui::SetNextWindowSize(ImVec2(initW, initH), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSizeConstraints(ImVec2(minW, minH), ImVec2(maxW, maxH));

			if (display.x > initW && display.y > initH)
			{
				ImGui::SetNextWindowPos(ImVec2((display.x - initW) * 0.5f, (display.y - initH) * 0.5f),
					ImGuiCond_FirstUseEver);
			}

			const std::string label = window.title + "##addon_" + instance->id + "_" + window.id;

			if (ImGui::Begin(label.c_str(), &window.open))
				CallNoArgs(*instance, window.drawRef, "ui");

			ImGui::End();
		}
	}
}

void LuaHost::OnGameStarted()
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);

	for (std::unique_ptr<Instance>& instance : g_instances)
	{
		for (int ref : instance->gameStartedCallbacks)
			CallNoArgs(*instance, ref, "game_started");
	}
}

void LuaHost::OnGameEnded(const GameResults::Report& report)
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);

	for (std::unique_ptr<Instance>& instance : g_instances)
	{
		for (int ref : instance->gameEndedCallbacks)
		{
			lua_State* const L = instance->state;
			lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

			lua_newtable(L);
			lua_pushstring(L, EndingString(report.ending));
			lua_setfield(L, -2, "ending");
			lua_pushinteger(L, report.winner);
			lua_setfield(L, -2, "winner");
			lua_pushinteger(L, report.localSide);
			lua_setfield(L, -2, "local_side");

			lua_newtable(L);
			lua_pushstring(L, DecimalId(report.steamId[0]).c_str());
			lua_rawseti(L, -2, 1);
			lua_pushstring(L, DecimalId(report.steamId[1]).c_str());
			lua_rawseti(L, -2, 2);
			lua_setfield(L, -2, "steam_id");

			lua_newtable(L);
			lua_pushinteger(L, report.chara[0]);
			lua_rawseti(L, -2, 1);
			lua_pushinteger(L, report.chara[1]);
			lua_rawseti(L, -2, 2);
			lua_setfield(L, -2, "chara");

			lua_pushlstring(L, reinterpret_cast<const char*>(report.record.data()), report.record.size());
			lua_setfield(L, -2, "record");

			if (lua_pcall(L, 1, 0, 0) != LUA_OK)
			{
				const char* const message = lua_tostring(L, -1);
				LOG_ERROR("Addon %s (game_ended): %s", instance->id.c_str(), message != nullptr ? message : "error");
				lua_pop(L, 1);
			}
		}
	}
}

void LuaHost::OnNetworkMenuOpened(int mode)
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);

	for (std::unique_ptr<Instance>& instance : g_instances)
	{
		for (int ref : instance->networkMenuOpenedCallbacks)
			CallOneInt(*instance, ref, static_cast<lua_Integer>(mode), "network_menu_opened");
	}
}

void LuaHost::OnPlayerMatchResult(uintptr_t root, int result)
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);

	for (std::unique_ptr<Instance>& instance : g_instances)
	{
		for (int ref : instance->playerMatchResultCallbacks)
			CallTwoInts(*instance, ref, static_cast<lua_Integer>(root), static_cast<lua_Integer>(result), "player_match_result");
	}
}

void LuaHost::UnloadAll()
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);
	g_hasLastScene = false;

	{
		std::lock_guard<std::mutex> lock(g_httpMutex);
		g_httpPending.clear();
		g_httpCompleted.clear();
	}

	for (std::unique_ptr<Instance>& instance : g_instances)
	{
		for (int ref : instance->shutdownCallbacks)
			CallNoArgs(*instance, ref, "shutdown");

		if (instance->state != nullptr)
		{
			lua_close(instance->state);
			instance->state = nullptr;
		}
	}

	g_instances.clear();
}

void LuaHost::Shutdown()
{
	{
		std::lock_guard<std::mutex> lock(g_httpMutex);
		g_httpStop = true;
		g_httpPending.clear();
		g_httpCompleted.clear();
	}
	g_httpWake.notify_all();

	UnloadAll();
}

bool LuaHost::AnyWindowOpen()
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);

	for (const std::unique_ptr<Instance>& instance : g_instances)
	{
		for (const AddonWindow& window : instance->windows)
		{
			if (window.open)
				return true;
		}
	}

	return false;
}

bool LuaHost::WantsHud()
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);

	for (const std::unique_ptr<Instance>& instance : g_instances)
	{
		if (!instance->hudCallbacks.empty())
			return true;
	}

	return false;
}

void LuaHost::DrawLauncherCards()
{
	std::lock_guard<std::recursive_mutex> vmLock(g_vmMutex);

	for (std::unique_ptr<Instance>& instance : g_instances)
	{
		for (AddonWindow& window : instance->windows)
		{
			const std::string cardId = "##addon_" + instance->id + "_" + window.id;
			const Cards::Scope card(cardId.c_str());

			if (!card)
				continue;

			if (Cards::TitleWithToggle(window.title.c_str(), window.open))
				window.open = !window.open;

			if (!window.summary.empty())
				UiText::Muted("%s", window.summary.c_str());
		}
	}
}
