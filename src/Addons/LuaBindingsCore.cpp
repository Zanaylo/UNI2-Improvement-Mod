#include "Addons/LuaHost.h"

#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/FileSystem.h"
#include "Core/Formats/Json.h"
#include "Core/Text/Language.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameDraw.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/GameState.h"
#include "Game/Engine/OnlineState.h"
#include "Game/Engine/SceneWatch.h"
#include "Game/Lobby/RoomAutomation.h"
#include "Game/Replays/ReplayFiles.h"
#include "Game/Tables/CharaTables.h"
#include "Network/NetLink.h"
#include "Network/Steam/RichPresence.h"
#include "Network/Steam/SteamInterfaces.h"
#include "Overlay/Hud/NotificationBar.h"
#include "Overlay/Hud/Toast.h"
#include "Palette/PaletteControl.h"

#include <Windows.h>
#include <imgui.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

uint64_t ReadU64Arg(lua_State* L, int index)
{
	if (lua_isinteger(L, index))
		return static_cast<uint64_t>(lua_tointeger(L, index));

	const char* const text = luaL_optstring(L, index, "0");
	return _strtoui64(text, nullptr, 10);
}

std::string DecimalU64(uint64_t value)
{
	char text[32] = {};
	sprintf_s(text, "%llu", static_cast<unsigned long long>(value));
	return text;
}

const char* LanguageCode(LanguageId id)
{
	switch (id)
	{
	case LanguageId::Portuguese:
		return "pt";
	case LanguageId::Spanish:
		return "es";
	default:
		return "en";
	}
}

ToastKind ParseToastKind(const char* kind)
{
	if (kind == nullptr)
		return ToastKind::Info;

	if (_stricmp(kind, "success") == 0)
		return ToastKind::Success;

	if (_stricmp(kind, "warning") == 0 || _stricmp(kind, "warn") == 0)
		return ToastKind::Warning;

	if (_stricmp(kind, "error") == 0)
		return ToastKind::Error;

	return ToastKind::Info;
}

int LuaLog(lua_State* L)
{
	const LuaHost::Instance* const owner = LuaHost::InstanceOf(L);
	const char* const msg = luaL_checkstring(L, 1);
	LOG("[%s] %s", owner != nullptr ? owner->id.c_str() : "addon", msg);
	return 0;
}

int LuaWarn(lua_State* L)
{
	const LuaHost::Instance* const owner = LuaHost::InstanceOf(L);
	const char* const msg = luaL_checkstring(L, 1);
	LOG_WARN("[%s] %s", owner != nullptr ? owner->id.c_str() : "addon", msg);
	return 0;
}

int LuaError(lua_State* L)
{
	const LuaHost::Instance* const owner = LuaHost::InstanceOf(L);
	const char* const msg = luaL_checkstring(L, 1);
	LOG_ERROR("[%s] %s", owner != nullptr ? owner->id.c_str() : "addon", msg);
	return 0;
}

int LuaTime(lua_State* L)
{
	lua_pushinteger(L, static_cast<lua_Integer>(GetTickCount64()));
	return 1;
}

int LuaClock(lua_State* L)
{
	lua_pushnumber(L, ImGui::GetCurrentContext() != nullptr ? ImGui::GetTime() : static_cast<double>(GetTickCount64()) / 1000.0);
	return 1;
}

int LuaVersion(lua_State* L)
{
	lua_pushstring(L, UNI2_IM_VERSION);
	return 1;
}

int LuaLanguage(lua_State* L)
{
	lua_pushstring(L, LanguageCode(Language::Current()));
	return 1;
}

int LuaTr(lua_State* L)
{
	const char* const text = luaL_checkstring(L, 1);
	lua_pushstring(L, Tr(text));
	return 1;
}

int LuaToast(lua_State* L)
{
	const char* const kind = luaL_checkstring(L, 1);
	const char* const title = luaL_checkstring(L, 2);
	const char* const text = luaL_checkstring(L, 3);
	Toast::Show(ParseToastKind(kind), title, "%s", text);
	return 0;
}

int LuaNotify(lua_State* L)
{
	const char* const text = luaL_checkstring(L, 1);
	NotificationBar::Add("%s", text);
	return 0;
}

int LuaOn(lua_State* L)
{
	LuaHost::Instance* const owner = LuaHost::InstanceOf(L);
	const char* const event = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	if (owner == nullptr)
		return 0;

	lua_pushvalue(L, 2);
	const int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	if (strcmp(event, "frame") == 0)
		owner->frameCallbacks.push_back(ref);
	else if (strcmp(event, "hud") == 0)
		owner->hudCallbacks.push_back(ref);
	else if (strcmp(event, "game_started") == 0)
		owner->gameStartedCallbacks.push_back(ref);
	else if (strcmp(event, "game_ended") == 0)
		owner->gameEndedCallbacks.push_back(ref);
	else if (strcmp(event, "scene_changed") == 0)
		owner->sceneChangedCallbacks.push_back(ref);
	else if (strcmp(event, "network_menu_opened") == 0)
		owner->networkMenuOpenedCallbacks.push_back(ref);
	else if (strcmp(event, "player_match_result") == 0)
		owner->playerMatchResultCallbacks.push_back(ref);
	else if (strcmp(event, "shutdown") == 0)
		owner->shutdownCallbacks.push_back(ref);
	else
		luaL_unref(L, LUA_REGISTRYINDEX, ref);

	return 0;
}

int LuaConfigGetString(lua_State* L)
{
	const char* const section = luaL_checkstring(L, 1);
	const char* const key = luaL_checkstring(L, 2);
	const char* const fallback = luaL_optstring(L, 3, "");

	char buffer[2048] = {};
	Ini::GetString(section, key, fallback, buffer, sizeof(buffer), Settings::GetIniPath().c_str());
	lua_pushstring(L, buffer);
	return 1;
}

int LuaConfigGetInt(lua_State* L)
{
	const char* const section = luaL_checkstring(L, 1);
	const char* const key = luaL_checkstring(L, 2);
	const int fallback = static_cast<int>(luaL_optinteger(L, 3, 0));

	const int value = static_cast<int>(Ini::GetInt(section, key, fallback, Settings::GetIniPath().c_str()));
	lua_pushinteger(L, value);
	return 1;
}

int LuaConfigSetString(lua_State* L)
{
	const char* const section = luaL_checkstring(L, 1);
	const char* const key = luaL_checkstring(L, 2);
	const char* const value = luaL_checkstring(L, 3);
	Settings::SaveString(section, key, value);
	return 0;
}

int LuaConfigSetInt(lua_State* L)
{
	const char* const section = luaL_checkstring(L, 1);
	const char* const key = luaL_checkstring(L, 2);
	const int value = static_cast<int>(luaL_checkinteger(L, 3));
	Settings::SaveInt(section, key, value);
	return 0;
}

int LuaCommandLine(lua_State* L)
{
	const char* const cmd = GetCommandLineA();
	lua_pushstring(L, cmd != nullptr ? cmd : "");
	return 1;
}

int LuaFsRead(lua_State* L)
{
	const char* const path = luaL_checkstring(L, 1);
	std::vector<uint8_t> bytes;

	if (!ReadWholeFile(path, bytes))
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushlstring(L, reinterpret_cast<const char*>(bytes.data()), bytes.size());
	return 1;
}

int LuaFsWrite(lua_State* L)
{
	const char* const path = luaL_checkstring(L, 1);
	size_t length = 0;
	const char* const data = luaL_checklstring(L, 2, &length);

	const std::vector<uint8_t> bytes(reinterpret_cast<const uint8_t*>(data),
		reinterpret_cast<const uint8_t*>(data) + length);
	lua_pushboolean(L, WriteWholeFile(path, bytes) ? 1 : 0);
	return 1;
}

int LuaFsExists(lua_State* L)
{
	const char* const path = luaL_checkstring(L, 1);
	lua_pushboolean(L, GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? 1 : 0);
	return 1;
}

int LuaFsMkdir(lua_State* L)
{
	const char* const path = luaL_checkstring(L, 1);
	lua_pushboolean(L, CreateDirectoryTree(path) ? 1 : 0);
	return 1;
}

int LuaFsAddonDir(lua_State* L)
{
	const LuaHost::Instance* const owner = LuaHost::InstanceOf(L);
	lua_pushstring(L, owner != nullptr ? owner->folder.c_str() : "");
	return 1;
}

int LuaFsModRoot(lua_State* L)
{
	lua_pushstring(L, GetModRootPath().c_str());
	return 1;
}

int LuaMemRva(lua_State* L)
{
	const uintptr_t rva = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
	lua_pushinteger(L, static_cast<lua_Integer>(RvaToAddress(rva)));
	return 1;
}

int LuaMemSig(lua_State* L)
{
	const uintptr_t rva = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
	lua_pushinteger(L, static_cast<lua_Integer>(CodeSignatures::Address(rva)));
	return 1;
}

int LuaMemReadI32(lua_State* L)
{
	const uintptr_t address = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
	uint32_t value = 0;

	if (address == 0 || !TryReadDword(reinterpret_cast<const void*>(address), value))
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushinteger(L, static_cast<int32_t>(value));
	return 1;
}

int LuaMemReadU32(lua_State* L)
{
	const uintptr_t address = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
	uint32_t value = 0;

	if (address == 0 || !TryReadDword(reinterpret_cast<const void*>(address), value))
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushinteger(L, static_cast<lua_Integer>(value));
	return 1;
}

int LuaMemReadU16(lua_State* L)
{
	const uintptr_t address = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
	uint16_t value = 0;

	if (address == 0 || !TryReadMemory(&value, reinterpret_cast<const void*>(address), sizeof(value)))
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushinteger(L, static_cast<lua_Integer>(value));
	return 1;
}

int LuaMemReadU8(lua_State* L)
{
	const uintptr_t address = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
	uint8_t value = 0;

	if (address == 0 || !TryReadMemory(&value, reinterpret_cast<const void*>(address), sizeof(value)))
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pushinteger(L, static_cast<lua_Integer>(value));
	return 1;
}

int LuaMemWriteI32(lua_State* L)
{
	const uintptr_t address = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
	const uint32_t value = static_cast<uint32_t>(static_cast<int32_t>(luaL_checkinteger(L, 2)));
	lua_pushboolean(L, address != 0 && TryWriteDword(reinterpret_cast<void*>(address), value) ? 1 : 0);
	return 1;
}

int LuaMemWriteU16(lua_State* L)
{
	const uintptr_t address = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
	const uint16_t value = static_cast<uint16_t>(luaL_checkinteger(L, 2));
	lua_pushboolean(L, address != 0 && TryWriteMemory(reinterpret_cast<void*>(address), &value, sizeof(value)) ? 1 : 0);
	return 1;
}

int LuaMemWriteU8(lua_State* L)
{
	const uintptr_t address = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
	const uint8_t value = static_cast<uint8_t>(luaL_checkinteger(L, 2));
	lua_pushboolean(L, address != 0 && TryWriteMemory(reinterpret_cast<void*>(address), &value, sizeof(value)) ? 1 : 0);
	return 1;
}

int LuaMemReadRvaI32(lua_State* L)
{
	const uintptr_t address = RvaToAddress(static_cast<uintptr_t>(luaL_checkinteger(L, 1)));
	uint32_t value = 0;

	if (address == 0 || !TryReadDword(reinterpret_cast<const void*>(address), value))
	{
		lua_pushinteger(L, 0);
		return 1;
	}

	lua_pushinteger(L, static_cast<int32_t>(value));
	return 1;
}

int LuaMemWriteRvaI32(lua_State* L)
{
	const uintptr_t address = RvaToAddress(static_cast<uintptr_t>(luaL_checkinteger(L, 1)));
	const uint32_t value = static_cast<uint32_t>(static_cast<int32_t>(luaL_checkinteger(L, 2)));
	lua_pushboolean(L, address != 0 && TryWriteDword(reinterpret_cast<void*>(address), value) ? 1 : 0);
	return 1;
}

int LuaGameScene(lua_State* L)
{
	lua_pushinteger(L, static_cast<lua_Integer>(SceneWatch::Current()));
	return 1;
}

int LuaGameSceneFrames(lua_State* L)
{
	lua_pushinteger(L, static_cast<lua_Integer>(SceneWatch::HeldFrames()));
	return 1;
}

int LuaGameInMatch(lua_State* L)
{
	lua_pushboolean(L, GameState::IsInMatch() ? 1 : 0);
	return 1;
}

int LuaGameIsTraining(lua_State* L)
{
	lua_pushboolean(L, GameState::IsTrainingBattle() ? 1 : 0);
	return 1;
}

int LuaGameIsOnline(lua_State* L)
{
	lua_pushboolean(L, OnlineState::IsOnline() ? 1 : 0);
	return 1;
}

int LuaGameIsSpectating(lua_State* L)
{
	lua_pushboolean(L, OnlineState::IsSpectating() ? 1 : 0);
	return 1;
}

int LuaGameLocalSide(lua_State* L)
{
	lua_pushinteger(L, PaletteControl::LocalPlayer());
	return 1;
}

int LuaGameCharacter(lua_State* L)
{
	const int side = static_cast<int>(luaL_checkinteger(L, 1));
	lua_pushinteger(L, GameState::GetLoadedCharacter(side));
	return 1;
}

int LuaGameCharacterName(lua_State* L)
{
	const int id = static_cast<int>(luaL_checkinteger(L, 1));
	const char* const name = CharaTables::Name(id);
	lua_pushstring(L, name != nullptr ? name : "");
	return 1;
}

int LuaGameClaimPad(lua_State* L)
{
	lua_pushinteger(L, RoomAutomation::ClaimNetworkPad());
	return 1;
}

int LuaGameRequestScene(lua_State* L)
{
	const uint32_t scene = static_cast<uint32_t>(luaL_checkinteger(L, 1));
	lua_pushboolean(L, RoomAutomation::RequestScene(scene) ? 1 : 0);
	return 1;
}

int LuaGameEnterNetwork(lua_State* L)
{
	lua_pushboolean(L, RoomAutomation::EnterNetwork() ? 1 : 0);
	return 1;
}

int LuaGameLeaveTraining(lua_State* L)
{
	lua_pushboolean(L, RoomAutomation::LeaveTraining() ? 1 : 0);
	return 1;
}

int LuaGameCanJoinRoom(lua_State* L)
{
	lua_pushboolean(L, RoomAutomation::CanJoin() ? 1 : 0);
	return 1;
}

int LuaGameJoinRoom(lua_State* L)
{
	const uint64_t lobby = ReadU64Arg(L, 1);
	const uint64_t owner = ReadU64Arg(L, 2);
	lua_pushboolean(L, RoomAutomation::Join(lobby, owner) ? 1 : 0);
	return 1;
}

int LuaGameRoomCreateStart(lua_State* L)
{
	const int firstTo = static_cast<int>(luaL_optinteger(L, 1, 0));
	RoomAutomation::StartCreate(firstTo);
	return 0;
}

int LuaGameRoomCreateSetFirstTo(lua_State* L)
{
	RoomAutomation::SetCreateFirstTo(static_cast<int>(luaL_checkinteger(L, 1)));
	return 0;
}

int LuaGameRoomCreateStop(lua_State* L)
{
	RoomAutomation::StopCreate();
	return 0;
}

int LuaGameRoomCreateStep(lua_State* L)
{
	lua_pushinteger(L, static_cast<int>(RoomAutomation::StepCreate()));
	return 1;
}

int LuaGameRoomCreateStage(lua_State* L)
{
	const RoomAutomation::CreateStage stage = RoomAutomation::CurrentCreateStage();
	lua_pushinteger(L, static_cast<int>(stage));
	lua_pushstring(L, RoomAutomation::CreateStageText(stage));
	return 2;
}

int LuaGameRoomCreateTimedOut(lua_State* L)
{
	lua_pushboolean(L, RoomAutomation::CreateTimedOut() ? 1 : 0);
	return 1;
}

int LuaSteamIsReady(lua_State* L)
{
	lua_pushboolean(L, SteamInterfaces::IsReady() ? 1 : 0);
	return 1;
}

int LuaSteamOwnId(lua_State* L)
{
	lua_pushstring(L, DecimalU64(SteamInterfaces::GetLocalSteamId()).c_str());
	return 1;
}

int LuaSteamLobby(lua_State* L)
{
	lua_pushstring(L, DecimalU64(NetLink::Lobby()).c_str());
	return 1;
}

int LuaSteamLobbyOwner(lua_State* L)
{
	const uint64_t lobby = ReadU64Arg(L, 1);
	lua_pushstring(L, DecimalU64(SteamInterfaces::GetLobbyOwner(lobby)).c_str());
	return 1;
}

int LuaSteamLobbyLimit(lua_State* L)
{
	const uint64_t lobby = ReadU64Arg(L, 1);
	lua_pushinteger(L, SteamInterfaces::GetLobbyMemberLimit(lobby));
	return 1;
}

int LuaSteamLaunchCmd(lua_State* L)
{
	char text[2048] = {};

	if (!SteamInterfaces::GetLaunchCommandLine(text, sizeof(text)))
	{
		lua_pushstring(L, "");
		return 1;
	}

	lua_pushstring(L, text);
	return 1;
}

int LuaSteamLaunchQuery(lua_State* L)
{
	const char* const key = luaL_checkstring(L, 1);
	lua_pushstring(L, SteamInterfaces::GetLaunchQueryParam(key));
	return 1;
}

int LuaSteamSetPresenceNote(lua_State* L)
{
	const char* const note = luaL_optstring(L, 1, "");
	RichPresence::SetNote(note);
	return 0;
}

int LuaReplayFolder(lua_State* L)
{
	lua_pushstring(L, ReplayFiles::GetFolder().c_str());
	return 1;
}

int LuaReplayCanPlay(lua_State* L)
{
	lua_pushboolean(L, ReplayFiles::CanPlay() ? 1 : 0);
	return 1;
}

int LuaReplayIsLoadingPatch(lua_State* L)
{
	lua_pushboolean(L, ReplayFiles::IsLoadingPatch() ? 1 : 0);
	return 1;
}

int LuaReplayRefresh(lua_State* L)
{
	ReplayFiles::Refresh();
	return 0;
}

int LuaReplaySaveRecord(lua_State* L)
{
	size_t length = 0;
	const char* const data = luaL_checklstring(L, 1, &length);
	const char* const note = luaL_optstring(L, 2, "");
	const char* const name0 = luaL_optstring(L, 3, "");
	const char* const name1 = luaL_optstring(L, 4, "");

	const std::vector<uint8_t> record(reinterpret_cast<const uint8_t*>(data),
		reinterpret_cast<const uint8_t*>(data) + length);
	const std::string names[2] = { name0, name1 };
	const std::string saved = ReplayFiles::SaveRecord(record, note, names);
	lua_pushstring(L, saved.c_str());
	return 1;
}

int LuaReplayDescribeHeader(lua_State* L)
{
	size_t length = 0;
	const char* const data = luaL_checklstring(L, 1, &length);

	if (length < ReplayFiles::kRecordSize)
	{
		lua_pushnil(L);
		return 1;
	}

	ReplayFiles::Info info = {};
	ReplayFiles::DescribeHeader(reinterpret_cast<const uint8_t*>(data), info);

	lua_newtable(L);
	lua_pushstring(L, DecimalU64(info.steamId[0]).c_str());
	lua_setfield(L, -2, "steam_id_0");
	lua_pushstring(L, DecimalU64(info.steamId[1]).c_str());
	lua_setfield(L, -2, "steam_id_1");
	lua_pushinteger(L, info.chara[0]);
	lua_setfield(L, -2, "chara_0");
	lua_pushinteger(L, info.chara[1]);
	lua_setfield(L, -2, "chara_1");
	return 1;
}

int LuaReplayMatchupOf(lua_State* L)
{
	const char* const path = luaL_checkstring(L, 1);
	lua_pushstring(L, ReplayFiles::MatchupOf(path).c_str());
	return 1;
}

int LuaReplayRequestPlayback(lua_State* L)
{
	const char* const path = luaL_checkstring(L, 1);
	std::string error;
	const bool ok = ReplayFiles::RequestPlayback(path, error);
	lua_pushboolean(L, ok ? 1 : 0);
	lua_pushstring(L, error.c_str());
	return 2;
}

int LuaHttpRequest(lua_State* L)
{
	LuaHost::Instance* const owner = LuaHost::InstanceOf(L);
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	HttpRequest::Request request;

	lua_getfield(L, 1, "method");
	request.method = luaL_optstring(L, -1, "GET");
	lua_pop(L, 1);

	lua_getfield(L, 1, "url");
	request.url = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "body");
	if (lua_isstring(L, -1))
	{
		size_t length = 0;
		const char* const body = lua_tolstring(L, -1, &length);
		request.body.assign(body, length);
	}
	lua_pop(L, 1);

	lua_getfield(L, 1, "headers");
	if (lua_istable(L, -1))
	{
		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			if (lua_isstring(L, -2) && lua_isstring(L, -1))
				request.headers.push_back({ lua_tostring(L, -2), lua_tostring(L, -1) });

			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_pushvalue(L, 2);
	const int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
	LuaHost::EnqueueHttp(owner, std::move(request), callbackRef);
	return 0;
}

void PushJsonValue(lua_State* L, const Json::Value& value)
{
	switch (value.GetType())
	{
	case Json::Value::Type_Bool:
		lua_pushboolean(L, value.AsBool() ? 1 : 0);
		return;
	case Json::Value::Type_Number:
	{
		const double number = value.AsNumber();
		const double floored = std::floor(number);

		if (number == floored && number >= -9007199254740992.0 && number <= 9007199254740992.0)
			lua_pushinteger(L, static_cast<lua_Integer>(number));
		else
			lua_pushnumber(L, number);
		return;
	}
	case Json::Value::Type_String:
	{
		const std::string text = value.AsString();
		lua_pushlstring(L, text.data(), text.size());
		return;
	}
	case Json::Value::Type_Array:
		lua_newtable(L);
		for (size_t i = 0; i < value.Count(); ++i)
		{
			PushJsonValue(L, *value.At(i));
			lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
		}
		return;
	case Json::Value::Type_Object:
		lua_newtable(L);
		for (const auto& member : value.Members())
		{
			PushJsonValue(L, member.second);
			lua_setfield(L, -2, member.first.c_str());
		}
		return;
	default:
		lua_pushnil(L);
		return;
	}
}

void AppendJsonEscaped(std::string& out, const char* data, size_t length)
{
	out.push_back('"');

	for (size_t i = 0; i < length; ++i)
	{
		const unsigned char c = static_cast<unsigned char>(data[i]);

		switch (c)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				char hex[8] = {};
				sprintf_s(hex, "\\u%04x", c);
				out += hex;
			}
			else
			{
				out.push_back(static_cast<char>(c));
			}
			break;
		}
	}

	out.push_back('"');
}

void EncodeJsonValue(lua_State* L, int index, std::string& out, int depth)
{
	if (depth > 32)
	{
		out += "null";
		return;
	}

	const int absIndex = lua_absindex(L, index);
	const int type = lua_type(L, absIndex);

	switch (type)
	{
	case LUA_TNIL:
		out += "null";
		return;
	case LUA_TBOOLEAN:
		out += lua_toboolean(L, absIndex) ? "true" : "false";
		return;
	case LUA_TNUMBER:
		if (lua_isinteger(L, absIndex))
		{
			char num[32] = {};
			sprintf_s(num, "%lld", static_cast<long long>(lua_tointeger(L, absIndex)));
			out += num;
		}
		else
		{
			char num[64] = {};
			sprintf_s(num, "%.15g", lua_tonumber(L, absIndex));
			out += num;
		}
		return;
	case LUA_TSTRING:
	{
		size_t length = 0;
		const char* const str = lua_tolstring(L, absIndex, &length);
		AppendJsonEscaped(out, str, length);
		return;
	}
	case LUA_TTABLE:
	{
		size_t count = 0;
		bool isArray = true;
		lua_Integer maxIndex = 0;

		lua_pushnil(L);
		while (lua_next(L, absIndex) != 0)
		{
			++count;

			if (lua_type(L, -2) == LUA_TNUMBER && lua_isinteger(L, -2))
			{
				const lua_Integer key = lua_tointeger(L, -2);
				if (key <= 0)
					isArray = false;
				else if (key > maxIndex)
					maxIndex = key;
			}
			else
			{
				isArray = false;
			}

			lua_pop(L, 1);
		}

		if (count == 0)
		{
			out += "{}";
			return;
		}

		if (isArray && static_cast<size_t>(maxIndex) == count)
		{
			out.push_back('[');
			for (lua_Integer i = 1; i <= maxIndex; ++i)
			{
				if (i > 1)
					out.push_back(',');

				lua_rawgeti(L, absIndex, i);
				EncodeJsonValue(L, -1, out, depth + 1);
				lua_pop(L, 1);
			}
			out.push_back(']');
			return;
		}

		out.push_back('{');
		bool first = true;
		lua_pushnil(L);
		while (lua_next(L, absIndex) != 0)
		{
			if (lua_type(L, -2) == LUA_TSTRING)
			{
				if (!first)
					out.push_back(',');

				first = false;
				size_t keyLen = 0;
				const char* const key = lua_tolstring(L, -2, &keyLen);
				AppendJsonEscaped(out, key, keyLen);
				out.push_back(':');
				EncodeJsonValue(L, -1, out, depth + 1);
			}

			lua_pop(L, 1);
		}
		out.push_back('}');
		return;
	}
	default:
		out += "null";
		return;
	}
}

int LuaJsonDecode(lua_State* L)
{
	size_t length = 0;
	const char* const text = luaL_checklstring(L, 1, &length);
	Json::Value root;

	if (!Json::Parse(std::string(text, length), root))
	{
		lua_pushnil(L);
		return 1;
	}

	PushJsonValue(L, root);
	return 1;
}

int LuaJsonEncode(lua_State* L)
{
	std::string out;
	EncodeJsonValue(L, 1, out, 0);
	lua_pushlstring(L, out.data(), out.size());
	return 1;
}

int LuaDrawFill(lua_State* L)
{
	const int x = static_cast<int>(luaL_checkinteger(L, 1));
	const int y = static_cast<int>(luaL_checkinteger(L, 2));
	const int w = static_cast<int>(luaL_checkinteger(L, 3));
	const int h = static_cast<int>(luaL_checkinteger(L, 4));
	const uint32_t colour = static_cast<uint32_t>(luaL_checkinteger(L, 5));
	const int layer = static_cast<int>(luaL_checkinteger(L, 6));
	lua_pushboolean(L, GameDraw::Fill(x, y, w, h, colour, layer) ? 1 : 0);
	return 1;
}

int LuaDrawFrame(lua_State* L)
{
	const int x = static_cast<int>(luaL_checkinteger(L, 1));
	const int y = static_cast<int>(luaL_checkinteger(L, 2));
	const int w = static_cast<int>(luaL_checkinteger(L, 3));
	const int h = static_cast<int>(luaL_checkinteger(L, 4));
	const uint32_t colour = static_cast<uint32_t>(luaL_checkinteger(L, 5));
	const int layer = static_cast<int>(luaL_checkinteger(L, 6));
	lua_pushboolean(L, GameDraw::Frame(x, y, w, h, colour, layer) ? 1 : 0);
	return 1;
}

int LuaDrawText(lua_State* L)
{
	const int align = static_cast<int>(luaL_checkinteger(L, 1));
	const int x = static_cast<int>(luaL_checkinteger(L, 2));
	const int y = static_cast<int>(luaL_checkinteger(L, 3));
	const char* const text = luaL_checkstring(L, 4);
	const uint32_t colour = static_cast<uint32_t>(luaL_checkinteger(L, 5));
	const int layer = static_cast<int>(luaL_checkinteger(L, 6));
	const int font = static_cast<int>(luaL_optinteger(L, 7, GameDraw::kCurrentFont));
	const int scale = static_cast<int>(luaL_optinteger(L, 8, 100));
	lua_pushboolean(L, GameDraw::Text(static_cast<GameDraw::Align>(align), x, y, text, colour, layer,
		{ font, scale }) ? 1 : 0);
	return 1;
}

void RegisterSubtable(lua_State* L, int parentIndex, const char* name, const luaL_Reg* funcs)
{
	lua_newtable(L);
	luaL_setfuncs(L, funcs, 0);
	lua_setfield(L, parentIndex, name);
}

void RegisterOffsets(lua_State* L, int parentIndex)
{
	lua_newtable(L);
	lua_pushinteger(L, GameOffsets::kSceneId); lua_setfield(L, -2, "SCENE_ID");
	lua_pushinteger(L, GameOffsets::kSceneRequest); lua_setfield(L, -2, "SCENE_REQUEST");
	lua_pushinteger(L, GameOffsets::kSceneBattle); lua_setfield(L, -2, "SCENE_BATTLE");
	lua_pushinteger(L, GameOffsets::kSceneMainMenu); lua_setfield(L, -2, "SCENE_MAIN_MENU");
	lua_pushinteger(L, GameOffsets::kSceneNetwork); lua_setfield(L, -2, "SCENE_NETWORK");
	lua_pushinteger(L, GameOffsets::kSceneCharaSelect); lua_setfield(L, -2, "SCENE_CHARA_SELECT");
	lua_pushinteger(L, GameOffsets::kSceneReplayList); lua_setfield(L, -2, "SCENE_REPLAY_LIST");
	lua_pushinteger(L, GameOffsets::kNetworkMenu); lua_setfield(L, -2, "NETWORK_MENU");
	lua_pushinteger(L, GameOffsets::kNetworkMenuState); lua_setfield(L, -2, "NETWORK_MENU_STATE");
	lua_pushinteger(L, GameOffsets::kNetworkMenuPending); lua_setfield(L, -2, "NETWORK_MENU_PENDING");
	lua_pushinteger(L, GameOffsets::kNetworkMenuList); lua_setfield(L, -2, "NETWORK_MENU_LIST");
	lua_pushinteger(L, GameOffsets::kNetworkMenuPick); lua_setfield(L, -2, "NETWORK_MENU_PICK");
	lua_pushinteger(L, GameOffsets::kNetworkMenuProfileCard); lua_setfield(L, -2, "NETWORK_MENU_PROFILE_CARD");
	lua_pushinteger(L, GameOffsets::kNetworkPickCreateRoom); lua_setfield(L, -2, "NETWORK_PICK_CREATE_ROOM");
	lua_pushinteger(L, GameOffsets::kProfileCardState); lua_setfield(L, -2, "PROFILE_CARD_STATE");
	lua_pushinteger(L, GameOffsets::kProfileCardDismiss); lua_setfield(L, -2, "PROFILE_CARD_DISMISS");
	lua_pushinteger(L, GameOffsets::kProfileCardDone); lua_setfield(L, -2, "PROFILE_CARD_DONE");
	lua_pushinteger(L, GameOffsets::kNetworkInitError); lua_setfield(L, -2, "NETWORK_INIT_ERROR");
	lua_pushinteger(L, GameOffsets::kNetworkStepState); lua_setfield(L, -2, "NETWORK_STEP_STATE");
	lua_pushinteger(L, GameOffsets::kNetworkStepErrorKind); lua_setfield(L, -2, "NETWORK_STEP_ERROR");
	lua_pushinteger(L, GameOffsets::kNetworkStepError); lua_setfield(L, -2, "NETWORK_STEP_ERROR_VALUE");
	lua_pushinteger(L, GameOffsets::kNetworkFlowError); lua_setfield(L, -2, "NETWORK_FLOW_ERROR");
	lua_pushinteger(L, GameOffsets::kRoomTaskState); lua_setfield(L, -2, "ROOM_TASK_STATE");
	lua_pushinteger(L, GameOffsets::kPlayerMatchRoot); lua_setfield(L, -2, "PLAYER_MATCH_ROOT");
	lua_pushinteger(L, GameOffsets::kPlayerMatchState); lua_setfield(L, -2, "PLAYER_MATCH_STATE");
	lua_pushinteger(L, GameOffsets::kPlayerMatchDialogStep); lua_setfield(L, -2, "PLAYER_MATCH_DIALOG_STEP");
	lua_pushinteger(L, GameOffsets::kPlayerMatchCreateMenu); lua_setfield(L, -2, "PLAYER_MATCH_CREATE_MENU");
	lua_pushinteger(L, GameOffsets::kPlayerMatchCreateAnswer); lua_setfield(L, -2, "PLAYER_MATCH_CREATE_ANSWER");
	lua_pushinteger(L, GameOffsets::kPlayerMatchRoomType); lua_setfield(L, -2, "PLAYER_MATCH_ROOM_TYPE");
	lua_pushinteger(L, GameOffsets::kPlayerMatchSettings); lua_setfield(L, -2, "PLAYER_MATCH_SETTINGS");
	lua_pushinteger(L, GameOffsets::kPlayerMatchAltSettings); lua_setfield(L, -2, "PLAYER_MATCH_ALT_SETTINGS");
	lua_pushinteger(L, GameOffsets::kPlayerMatchUseAltSettings); lua_setfield(L, -2, "PLAYER_MATCH_USE_ALT_SETTINGS");
	lua_pushinteger(L, GameOffsets::kPlayerMatchCreateScreen); lua_setfield(L, -2, "PLAYER_MATCH_CREATE_SCREEN");
	lua_pushinteger(L, GameOffsets::kPlayerMatchDialogOpen); lua_setfield(L, -2, "PLAYER_MATCH_DIALOG_OPEN");
	lua_pushinteger(L, GameOffsets::kCreateMenuChoosing); lua_setfield(L, -2, "CREATE_MENU_CHOOSING");
	lua_pushinteger(L, GameOffsets::kCreateMenuUnanswered); lua_setfield(L, -2, "CREATE_MENU_UNANSWERED");
	lua_pushinteger(L, GameOffsets::kCreateMenuCreateRoom); lua_setfield(L, -2, "CREATE_MENU_CREATE_ROOM");
	lua_pushinteger(L, GameOffsets::kMenuPages); lua_setfield(L, -2, "MENU_PAGES");
	lua_pushinteger(L, GameOffsets::kMenuPageStride); lua_setfield(L, -2, "MENU_PAGE_STRIDE");
	lua_pushinteger(L, GameOffsets::kMenuRowStride); lua_setfield(L, -2, "MENU_ROW_STRIDE");
	lua_pushinteger(L, GameOffsets::kMenuRowValue); lua_setfield(L, -2, "MENU_ROW_VALUE");
	lua_pushinteger(L, GameOffsets::kRoomCapacityRow); lua_setfield(L, -2, "ROOM_CAPACITY_ROW");
	lua_pushinteger(L, GameOffsets::kRoomCapacityTwoPeople); lua_setfield(L, -2, "ROOM_CAPACITY_TWO_PEOPLE");
	lua_pushinteger(L, GameOffsets::kRoomRematchRow); lua_setfield(L, -2, "ROOM_REMATCH_ROW");
	lua_pushinteger(L, GameOffsets::kRoomRoundsRow); lua_setfield(L, -2, "ROOM_ROUNDS_ROW");
	lua_pushinteger(L, GameOffsets::kRoomRoundTimeRow); lua_setfield(L, -2, "ROOM_ROUND_TIME_ROW");
	lua_pushinteger(L, GameOffsets::kRoomType1RematchRow); lua_setfield(L, -2, "ROOM_TYPE1_REMATCH_ROW");
	lua_pushinteger(L, GameOffsets::kRoomType1RoundsRow); lua_setfield(L, -2, "ROOM_TYPE1_ROUNDS_ROW");
	lua_pushinteger(L, GameOffsets::kRoomType1RoundTimeRow); lua_setfield(L, -2, "ROOM_TYPE1_ROUND_TIME_ROW");
	lua_pushinteger(L, GameOffsets::kRoomAltRematchRow); lua_setfield(L, -2, "ROOM_ALT_REMATCH_ROW");
	lua_pushinteger(L, GameOffsets::kRoomAltRoundsRow); lua_setfield(L, -2, "ROOM_ALT_ROUNDS_ROW");
	lua_pushinteger(L, GameOffsets::kRoomAltRoundTimeRow); lua_setfield(L, -2, "ROOM_ALT_ROUND_TIME_ROW");
	lua_pushinteger(L, GameOffsets::kRoomRoundsTwo); lua_setfield(L, -2, "ROOM_ROUNDS_TWO");
	lua_pushinteger(L, GameOffsets::kRoomRoundTimeDefault99); lua_setfield(L, -2, "ROOM_ROUND_TIME_DEFAULT_99");
	lua_pushinteger(L, GameOffsets::kMatchKind); lua_setfield(L, -2, "MATCH_KIND");
	lua_pushinteger(L, GameOffsets::kMatchKindRank); lua_setfield(L, -2, "MATCH_KIND_RANK");
	lua_pushinteger(L, GameOffsets::kMatchKindPlayer); lua_setfield(L, -2, "MATCH_KIND_PLAYER");
	lua_setfield(L, parentIndex, "offsets");
}

}

void LuaHost::RegisterCoreBindings(lua_State* L)
{
	static const luaL_Reg kRoot[] = {
		{ "log", LuaLog },
		{ "warn", LuaWarn },
		{ "error", LuaError },
		{ "time", LuaTime },
		{ "clock", LuaClock },
		{ "version", LuaVersion },
		{ "language", LuaLanguage },
		{ "tr", LuaTr },
		{ "toast", LuaToast },
		{ "notify", LuaNotify },
		{ "on", LuaOn },
		{ nullptr, nullptr }
	};

	static const luaL_Reg kConfig[] = {
		{ "get_string", LuaConfigGetString },
		{ "get_int", LuaConfigGetInt },
		{ "set_string", LuaConfigSetString },
		{ "set_int", LuaConfigSetInt },
		{ "command_line", LuaCommandLine },
		{ nullptr, nullptr }
	};

	static const luaL_Reg kFs[] = {
		{ "read", LuaFsRead },
		{ "write", LuaFsWrite },
		{ "exists", LuaFsExists },
		{ "mkdir", LuaFsMkdir },
		{ "addon_dir", LuaFsAddonDir },
		{ "mod_root", LuaFsModRoot },
		{ nullptr, nullptr }
	};

	static const luaL_Reg kMem[] = {
		{ "rva", LuaMemRva },
		{ "sig", LuaMemSig },
		{ "read_i32", LuaMemReadI32 },
		{ "read_u32", LuaMemReadU32 },
		{ "read_u16", LuaMemReadU16 },
		{ "read_u8", LuaMemReadU8 },
		{ "write_i32", LuaMemWriteI32 },
		{ "write_u16", LuaMemWriteU16 },
		{ "write_u8", LuaMemWriteU8 },
		{ "read_rva_i32", LuaMemReadRvaI32 },
		{ "write_rva_i32", LuaMemWriteRvaI32 },
		{ nullptr, nullptr }
	};

	static const luaL_Reg kGame[] = {
		{ "scene", LuaGameScene },
		{ "scene_frames", LuaGameSceneFrames },
		{ "in_match", LuaGameInMatch },
		{ "is_training", LuaGameIsTraining },
		{ "is_online", LuaGameIsOnline },
		{ "is_spectating", LuaGameIsSpectating },
		{ "local_side", LuaGameLocalSide },
		{ "character", LuaGameCharacter },
		{ "character_name", LuaGameCharacterName },
		{ "claim_pad", LuaGameClaimPad },
		{ "request_scene", LuaGameRequestScene },
		{ "enter_network", LuaGameEnterNetwork },
		{ "leave_training", LuaGameLeaveTraining },
		{ "can_join_room", LuaGameCanJoinRoom },
		{ "join_room", LuaGameJoinRoom },
		{ "room_create_start", LuaGameRoomCreateStart },
		{ "room_create_set_first_to", LuaGameRoomCreateSetFirstTo },
		{ "room_create_stop", LuaGameRoomCreateStop },
		{ "room_create_step", LuaGameRoomCreateStep },
		{ "room_create_stage", LuaGameRoomCreateStage },
		{ "room_create_timed_out", LuaGameRoomCreateTimedOut },
		{ nullptr, nullptr }
	};

	static const luaL_Reg kSteam[] = {
		{ "is_ready", LuaSteamIsReady },
		{ "own_id", LuaSteamOwnId },
		{ "lobby", LuaSteamLobby },
		{ "lobby_owner", LuaSteamLobbyOwner },
		{ "lobby_limit", LuaSteamLobbyLimit },
		{ "launch_command_line", LuaSteamLaunchCmd },
		{ "launch_query", LuaSteamLaunchQuery },
		{ "set_rich_presence_note", LuaSteamSetPresenceNote },
		{ nullptr, nullptr }
	};

	static const luaL_Reg kReplay[] = {
		{ "folder", LuaReplayFolder },
		{ "can_play", LuaReplayCanPlay },
		{ "is_loading_patch", LuaReplayIsLoadingPatch },
		{ "refresh", LuaReplayRefresh },
		{ "save_record", LuaReplaySaveRecord },
		{ "describe_header", LuaReplayDescribeHeader },
		{ "matchup_of", LuaReplayMatchupOf },
		{ "request_playback", LuaReplayRequestPlayback },
		{ nullptr, nullptr }
	};

	static const luaL_Reg kHttp[] = {
		{ "request", LuaHttpRequest },
		{ "json_decode", LuaJsonDecode },
		{ "json_encode", LuaJsonEncode },
		{ nullptr, nullptr }
	};

	static const luaL_Reg kDraw[] = {
		{ "fill", LuaDrawFill },
		{ "frame", LuaDrawFrame },
		{ "text", LuaDrawText },
		{ nullptr, nullptr }
	};

	lua_newtable(L);
	luaL_setfuncs(L, kRoot, 0);

	const int rootIndex = lua_gettop(L);
	RegisterSubtable(L, rootIndex, "config", kConfig);
	RegisterSubtable(L, rootIndex, "fs", kFs);
	RegisterSubtable(L, rootIndex, "mem", kMem);
	RegisterSubtable(L, rootIndex, "game", kGame);
	RegisterSubtable(L, rootIndex, "steam", kSteam);
	RegisterSubtable(L, rootIndex, "replay", kReplay);
	RegisterSubtable(L, rootIndex, "http", kHttp);
	RegisterSubtable(L, rootIndex, "draw", kDraw);
	RegisterOffsets(L, rootIndex);

	lua_setglobal(L, "uni2");
}
