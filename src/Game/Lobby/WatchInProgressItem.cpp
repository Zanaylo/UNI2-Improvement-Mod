#include "Game/Lobby/WatchInProgressItem.h"

#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Lobby/WatchItemRule.h"
#include "Hooks/GameHook.h"
#include "Network/ModPresence.h"
#include "Network/NetLink.h"
#include "Network/NetLog.h"
#include "Network/RoomWatch/RoomWatchViewer.h"

#include <cstdint>
#include <cstring>

namespace {

typedef void(__fastcall* MenuDrawFn)(void*);
typedef void(__fastcall* MenuCloseFn)(void*);
typedef int(__fastcall* TextDrawFn)(void*, void*, int, int, int, const char*, uint32_t, int);
typedef void(__fastcall* FirstItemFn)(void*, void*, int);

constexpr const char* kLabel = "Watch Match in Progress";
constexpr const char* kExpectedFirstLabel = "<GR_MS_Network_ViewMemberList>";
constexpr int kLeastModMembers = 2;
constexpr uint8_t kHiddenFlag = 1;
constexpr uint8_t kShownFlag = 0;
constexpr int kClosing = 1;

enum class LabelCheck
{
	Unchecked,
	Matches,
	Differs,
};

GameHook<MenuDrawFn> g_menuDrawHook("RoomExtraMenuDraw");
GameHook<TextDrawFn> g_textDrawHook("TextDraw");
GameHook<FirstItemFn> g_firstItemHook("RoomExtraMenuFirstItem");

bool g_hooked = false;
bool g_unavailable = false;
bool g_offering = false;
LabelCheck g_labelCheck = LabelCheck::Unchecked;
const char* g_firstLabel = nullptr;
WatchItemFlag g_flag;

uintptr_t Menu()
{
	return RvaToAddress(GameOffsets::kRoomExtraMenu);
}

const char** Labels()
{
	return reinterpret_cast<const char**>(RvaToAddress(GameOffsets::kRoomExtraMenuLabels));
}

uint8_t* FirstHiddenFlag()
{
	return reinterpret_cast<uint8_t*>(Menu() + GameOffsets::kRoomExtraMenuHidden);
}

int ReadGameInt(uintptr_t rva)
{
	int value = 0;
	TryReadMemory(&value, reinterpret_cast<const void*>(RvaToAddress(rva)), sizeof(value));

	return value;
}

bool IsSeated()
{
	uint16_t order[2] = {};
	uint16_t local = 0;

	TryReadMemory(order, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kRoomOrder)), sizeof(order));
	TryReadMemory(&local, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kRoomLocalMember)), sizeof(local));

	return local == order[0] || local == order[1];
}

bool FirstLabelIsTheGames()
{
	if (g_labelCheck != LabelCheck::Unchecked)
		return g_labelCheck == LabelCheck::Matches;

	const char* label = nullptr;

	if (!TryReadMemory(&label, Labels(), sizeof(label)) || label == nullptr)
		return false;

	char text[64] = {};

	if (!TryReadMemory(text, label, sizeof(text) - 1))
		return false;

	g_firstLabel = label;
	g_labelCheck = strncmp(text, kExpectedFirstLabel, strlen(kExpectedFirstLabel)) == 0 ? LabelCheck::Matches :
		LabelCheck::Differs;

	if (g_labelCheck == LabelCheck::Differs)
		LOG("WatchInProgressItem: the room menu's first item is not the one this build knows, the item stays off");

	return g_labelCheck == LabelCheck::Matches;
}

WatchItemRule::Room ReadRoom()
{
	const NetLink::Snapshot& link = NetLink::Current();

	WatchItemRule::Room room = {};
	room.enabled = g_modVals.joinInProgress;
	room.inRoom = link.lobby != 0 && link.backend == NetLink::Backend_None;
	room.onRoomScreen = ReadGameInt(GameOffsets::kSceneId) == static_cast<int>(GameOffsets::kSceneNetwork);
	room.roomIdle = ReadGameInt(GameOffsets::kRoomTaskState) == GameOffsets::kRoomTaskIdle;
	room.seated = IsSeated();
	room.otherModMembers = ModPresence::ModCount() >= kLeastModMembers;
	room.watching = RoomWatchViewer::IsBusy();

	return room;
}

void PutFirstLabel(const char* label)
{
	TryWriteMemory(Labels(), &label, sizeof(label));
}

void __fastcall HookedMenuDraw(void* menu)
{
	if (!g_offering)
	{
		g_menuDrawHook.Original()(menu);
		return;
	}

	PutFirstLabel(kLabel);
	g_menuDrawHook.Original()(menu);
	PutFirstLabel(g_firstLabel);
}

int __fastcall HookedTextDraw(void* font, void* edx, int mode, int x, int y, const char* text, uint32_t colour, int layer)
{
	const int result = g_textDrawHook.Original()(font, edx, mode, x, y, text, colour, layer);

	if (text == kLabel)
		PutFirstLabel(g_firstLabel);

	return result;
}

void CloseMenu()
{
	const int closing = kClosing;
	TryWriteMemory(reinterpret_cast<void*>(Menu() + GameOffsets::kRoomExtraMenuClosing), &closing, sizeof(closing));
	reinterpret_cast<MenuCloseFn>(CodeSignatures::Address(GameOffsets::kFnRoomExtraMenuClose))(reinterpret_cast<void*>(Menu()));
}

void __fastcall HookedFirstItem(void* self, void* edx, int mode)
{
	if (!g_offering)
	{
		g_firstItemHook.Original()(self, edx, mode);
		return;
	}

	NetLog::Write("room menu: Watch Match in Progress chosen");
	RoomWatchViewer::WatchInProgress();
	CloseMenu();
}

template <typename Fn, typename Handler>
bool Hook(GameHook<Fn>& hook, uintptr_t rva, Handler handler)
{
	if (hook.IsLive())
		return hook.SetEnabled(true);

	if (hook.InstallRva(rva, handler))
		return true;

	LOG("WatchInProgressItem: %s is not where this game version expects it", hook.Label());
	return false;
}

void ParkHooks()
{
	g_menuDrawHook.SetEnabled(false);
	g_textDrawHook.SetEnabled(false);
	g_firstItemHook.SetEnabled(false);
	g_hooked = false;
}

bool SetHooks(bool active)
{
	if (active == g_hooked)
		return g_hooked;

	if (!active)
	{
		ParkHooks();
		return false;
	}

	if (g_unavailable)
		return false;

	g_hooked = Hook(g_menuDrawHook, GameOffsets::kFnRoomExtraMenuDraw, &HookedMenuDraw) &&
		Hook(g_textDrawHook, GameOffsets::kFnTextDraw, &HookedTextDraw) &&
		Hook(g_firstItemHook, GameOffsets::kFnRoomExtraMenuFirstItem, &HookedFirstItem);

	if (g_hooked)
		return true;

	ParkHooks();
	g_unavailable = true;
	NetLog::Write("room menu: Watch Match in Progress is off for this session, its hooks could not be installed");
	return false;
}

void WriteFirstFlag(uint8_t value)
{
	TryWriteMemory(FirstHiddenFlag(), &value, sizeof(value));
}

void ApplyFlag(bool offer)
{
	uint8_t current = kShownFlag;
	TryReadMemory(&current, FirstHiddenFlag(), sizeof(current));

	switch (g_flag.Update(offer, current != kShownFlag))
	{
	case WatchItemFlag::Write_Show:
		WriteFirstFlag(kShownFlag);
		return;
	case WatchItemFlag::Write_Restore:
		WriteFirstFlag(kHiddenFlag);
		return;
	default:
		return;
	}
}

void Announce(bool offer)
{
	if (offer == g_offering)
		return;

	NetLog::Write("room menu: Watch Match in Progress %s", offer ? "offered" : "withdrawn");
}

}

void WatchInProgressItem::OnFrame()
{
	const bool wanted = WatchItemRule::ShouldOffer(ReadRoom()) && FirstLabelIsTheGames();
	const bool offer = wanted && SetHooks(true);

	Announce(offer);
	g_offering = offer;
	ApplyFlag(offer);

	if (!offer && !g_flag.IsShowing())
		SetHooks(false);
}

bool WatchInProgressItem::IsOffered()
{
	return g_offering;
}
