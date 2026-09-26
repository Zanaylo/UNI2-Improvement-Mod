#include "Game/Menus/TrainingMenu.h"

#include "Core/ThreadRole.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/GameHook.h"
#include "Hooks/HookManager.h"
#include "Game/Engine/CodeSignatures.h"

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <intrin.h>

namespace {

constexpr DWORD kActiveMs = 250;
constexpr int kLastSlot = GameOffsets::kTrainingMenuPageCount - 1;
constexpr int kExtraPage = GameOffsets::kTrainingMenuPageCount;
constexpr int kLogicalPages = GameOffsets::kTrainingMenuPageCount + 1;
constexpr int kAppendFaulted = -1;
constexpr int kMaxRows = 32;

struct MenuItem
{
	uint32_t head;
	const char* word;
	const char* info;
	int id;
	int type;
	uint32_t colour;
	int shown;
	int maximum;
	int gauge;
	int current;
	MenuItem** choicesBegin;
	MenuItem** choicesEnd;
	MenuItem** choicesCapacity;
};

static_assert(sizeof(MenuItem) == 0x34, "the game's menu item is 0x34 bytes");

struct GameVector
{
	void** begin;
	void** end;
	void** capacity;

	int Count() const
	{
		return static_cast<int>(end - begin);
	}
};

static_assert(sizeof(GameVector) == GameOffsets::kTrainingMenuVectorStride,
	"the menu's page vectors are 12 bytes apart");

struct RowList
{
	int rows[kMaxRows];
	int count;
	int title;
};

struct ExtraPage
{
	bool ready;
	bool shown;
	RowList game;
	RowList mod;
};

struct PageMemory
{
	bool wasOnExtra;
	bool restorePending;
};

struct WindowState
{
	int active;
	int shown;
};

typedef int(__fastcall* BuildFn)(void* self, void* unused);
typedef void(__fastcall* UpdateFn)(void* self, void* unused);
typedef int(__fastcall* OpenPickerFn)(void* self, void* unused, int id, int flag);
typedef int(__fastcall* TurnPageFn)(void* self, void* unused, int delta);
typedef int(__fastcall* ResetPageFn)(void* self, void* unused, int page);
typedef void(__fastcall* PushItemFn)(void* self, void* unused, const MenuItem* item);
typedef void*(__fastcall* ReallocateFn)(void* self, void* unused, void* where, const void* value);

GameHook<BuildFn> g_buildHook("TrainingMenuBuild");
GameHook<UpdateFn> g_updateHook("TrainingMenuUpdate");
GameHook<OpenPickerFn> g_pickerHook("TrainingMenuOpenPicker");
GameHook<TurnPageFn> g_turnHook("TrainingMenuTurnPage");
GameHook<ResetPageFn> g_resetHook("TrainingMenuResetPage");
void* g_dotsOriginal = nullptr;

TrainingMenu::IClient* g_client = nullptr;
std::atomic<TrainingMenu::IModal*> g_modal{ nullptr };
std::atomic<DWORD> g_lastUpdate{ 0 };
void* g_menu = nullptr;
ExtraPage g_extra = {};
std::atomic<void*> g_owner{ nullptr };
PageMemory g_memory = {};
WindowState g_heldWindow = {};

std::atomic<long> g_builds{ 0 };
char g_status[160] = "the training menu is not where this game version expects it";

template <typename T>
T& Field(void* menu, uintptr_t offset)
{
	return *reinterpret_cast<T*>(static_cast<uint8_t*>(menu) + offset);
}

GameVector* Pages(void* menu)
{
	return &Field<GameVector>(menu, GameOffsets::kTrainingMenuPages);
}

GameVector* SelectableRows(void* menu)
{
	return &Field<GameVector>(menu, GameOffsets::kTrainingMenuSelectable);
}

GameVector& Titles(void* menu)
{
	return Field<GameVector>(menu, GameOffsets::kTrainingMenuTitles);
}

int& SlotTitle(void* menu)
{
	return reinterpret_cast<int*>(Titles(menu).begin)[kLastSlot];
}

bool Intact(void* menu)
{
	if (Field<int>(menu, GameOffsets::kTrainingMenuBuilt) != 1)
		return false;

	const GameVector& titles = Titles(menu);
	const GameVector& rows = SelectableRows(menu)[kLastSlot];

	return titles.begin != nullptr && titles.Count() >= GameOffsets::kTrainingMenuPageCount &&
		rows.begin != nullptr && rows.end >= rows.begin;
}

bool Owns(void* menu)
{
	return menu != nullptr && g_extra.ready && g_owner.load() == menu && Intact(menu);
}

bool OwnsLiveMenu()
{
	void* live = nullptr;

	if (!TryReadMemory(&live, reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kTrainingMenuInstance)),
		sizeof(live)))
		return false;

	return live != nullptr && g_owner.load() == live && g_extra.ready;
}

int ChoiceCount(const MenuItem& item)
{
	return item.choicesBegin == nullptr ? 0 : static_cast<int>(item.choicesEnd - item.choicesBegin);
}

MenuItem* FindItem(void* menu, int id)
{
	GameVector* const pages = Pages(menu);

	for (int page = 0; page < GameOffsets::kTrainingMenuPageCount; ++page)
	{
		for (void** at = pages[page].begin; at != pages[page].end; ++at)
		{
			MenuItem* const item = static_cast<MenuItem*>(*at);

			if (item != nullptr && item->id == id)
				return item;
		}
	}

	return nullptr;
}

MenuItem* FindList(void* menu, int id)
{
	if (menu == nullptr)
		return nullptr;

	MenuItem* const item = FindItem(menu, id);

	if (item == nullptr || item->type != GameOffsets::kMenuItemTypeList)
		return nullptr;

	return item;
}

MenuItem Blank()
{
	MenuItem item = {};
	item.id = TrainingMenu::kNoValue;
	item.colour = GameOffsets::kMenuItemDefaultColour;
	item.shown = 1;

	return item;
}

template <typename Fn>
Fn GameFunction(uintptr_t rva)
{
	const uintptr_t address = CodeSignatures::Address(rva);

	return IsAddressInGameModule(address) ? reinterpret_cast<Fn>(address) : nullptr;
}

bool PushItem(GameVector& page, const MenuItem& item)
{
	const PushItemFn push = GameFunction<PushItemFn>(GameOffsets::kFnMenuItemPush);

	if (push == nullptr)
		return false;

	const int before = page.Count();
	push(&page, nullptr, &item);

	return page.Count() == before + 1;
}

bool PushRow(GameVector& rows, int index)
{
	if (rows.end != rows.capacity)
	{
		*reinterpret_cast<int*>(rows.end) = index;
		++rows.end;
		return true;
	}

	const ReallocateFn reallocate = GameFunction<ReallocateFn>(GameOffsets::kFnVectorReallocate);

	if (reallocate == nullptr)
		return false;

	reallocate(&rows, nullptr, rows.end, &index);
	return true;
}

void ReadRows(const GameVector& rows, RowList& out)
{
	out.count = 0;

	for (void** at = rows.begin; at != rows.end && out.count < kMaxRows; ++at)
		out.rows[out.count++] = *reinterpret_cast<int*>(at);
}

void AssignRows(GameVector& rows, const RowList& list)
{
	rows.end = rows.begin;

	for (int i = 0; i < list.count; ++i)
		PushRow(rows, list.rows[i]);
}

void ClampCursor(void* menu)
{
	const int page = Field<int>(menu, GameOffsets::kTrainingMenuPage);

	if (page < 0 || page >= GameOffsets::kTrainingMenuPageCount)
		return;

	const int count = SelectableRows(menu)[page].Count();
	int& cursor = Field<int>(menu, GameOffsets::kTrainingMenuCursor);

	if (cursor >= 0 && cursor < count)
		return;

	cursor = 0;
	Field<int>(menu, GameOffsets::kTrainingMenuCursorShown) = 0;
}

bool SwapRows(void* menu, const RowList& list)
{
	__try
	{
		AssignRows(SelectableRows(menu)[kLastSlot], list);
		SlotTitle(menu) = list.title;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

void Disown(const char* why)
{
	g_owner.store(nullptr);
	g_extra.ready = false;

	sprintf_s(g_status, "the mod's page was withdrawn: %s", why);
	LOG("TrainingMenu: %s", g_status);
}

void ShowExtra(void* menu, bool wanted)
{
	if (!Owns(menu) || g_extra.shown == wanted)
		return;

	if (!SwapRows(menu, wanted ? g_extra.mod : g_extra.game))
	{
		Disown("swapping its rows faulted");
		return;
	}

	g_extra.shown = wanted;
	ClampCursor(menu);
}

int LogicalPage(void* menu)
{
	const int page = Field<int>(menu, GameOffsets::kTrainingMenuPage);

	return page == kLastSlot && Owns(menu) && g_extra.shown ? kExtraPage : page;
}

void RememberPage(void* menu)
{
	if (!Owns(menu) || Field<int>(menu, GameOffsets::kTrainingMenuWindowActive) == 0)
		return;

	if (g_memory.restorePending)
	{
		g_memory.restorePending = false;

		if (g_memory.wasOnExtra && Field<int>(menu, GameOffsets::kTrainingMenuPage) == kLastSlot)
			ShowExtra(menu, true);
	}

	g_memory.wasOnExtra = LogicalPage(menu) == kExtraPage;
}

int Wrap(int value, int count)
{
	return (value % count + count) % count;
}

int Physical(int logical)
{
	return logical < kLastSlot ? logical : kLastSlot;
}

bool Append(GameVector& page, const TrainingMenu::ItemSpec& spec)
{
	const int count = spec.choiceCount < TrainingMenu::kMaxChoices ? spec.choiceCount
		: TrainingMenu::kMaxChoices;

	MenuItem choices[TrainingMenu::kMaxChoices] = {};
	MenuItem* pointers[TrainingMenu::kMaxChoices] = {};

	for (int i = 0; i < count; ++i)
	{
		choices[i] = Blank();
		choices[i].word = spec.choices[i];
		choices[i].id = i;
		pointers[i] = &choices[i];
	}

	MenuItem item = Blank();
	item.word = spec.word;
	item.info = spec.info;
	item.id = spec.id;
	item.type = GameOffsets::kMenuItemTypeList;
	item.current = spec.value >= 0 && spec.value < count ? spec.value : 0;
	item.choicesBegin = pointers;
	item.choicesEnd = pointers + count;
	item.choicesCapacity = pointers + count;

	return PushItem(page, item);
}

bool AppendTitle(GameVector& page, const TrainingMenu::PageSpec& spec)
{
	MenuItem title = Blank();
	title.word = spec.title;
	title.info = spec.info;
	title.type = GameOffsets::kMenuItemTypeTitle;

	return PushItem(page, title);
}

int AppendPage(void* menu)
{
	const int count = g_client->ItemCount() < TrainingMenu::kMaxItems ? g_client->ItemCount()
		: TrainingMenu::kMaxItems;

	if (count <= 0 || FindItem(menu, g_client->Item(0).id) != nullptr)
		return 0;

	GameVector& page = Pages(menu)[kLastSlot];

	ReadRows(SelectableRows(menu)[kLastSlot], g_extra.game);
	g_extra.game.title = SlotTitle(menu);

	if (!AppendTitle(page, g_client->Page()))
		return 0;

	g_extra.mod.title = page.Count() - 1;
	g_extra.mod.count = 0;

	for (int i = 0; i < count; ++i)
	{
		if (!Append(page, g_client->Item(i)))
			break;

		g_extra.mod.rows[g_extra.mod.count++] = page.Count() - 1;
	}

	g_extra.ready = g_extra.mod.count > 0;
	g_extra.shown = false;

	return g_extra.mod.count;
}

int AppendPageGuarded(void* menu)
{
	__try
	{
		return AppendPage(menu);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		g_extra.ready = false;
		return kAppendFaulted;
	}
}

void HoldWindow(void* menu)
{
	int& active = Field<int>(menu, GameOffsets::kTrainingMenuWindowActive);
	int& shown = Field<int>(menu, GameOffsets::kTrainingMenuWindowShown);

	g_heldWindow = { active, shown };
	active = 0;
	shown = 0;
}

void ReleaseWindow(void* menu)
{
	Field<int>(menu, GameOffsets::kTrainingMenuWindowActive) = g_heldWindow.active;
	Field<int>(menu, GameOffsets::kTrainingMenuWindowShown) = g_heldWindow.shown;
}

void CloseModal(void* menu)
{
	TrainingMenu::IModal* const modal = g_modal.exchange(nullptr);

	if (modal == nullptr)
		return;

	modal->Close();

	if (menu != nullptr)
		ReleaseWindow(menu);
}

int Player(void* menu)
{
	return Field<int>(menu, GameOffsets::kTrainingMenuPlayer);
}

bool HoldForModal(void* menu)
{
	TrainingMenu::IModal* const modal = g_modal.load();

	if (modal == nullptr)
		return false;

	if (modal->IsOpen())
	{
		MenuInput::State input = {};
		MenuInput::Read(Player(menu), input);
		modal->Update(input);
	}

	if (!modal->IsOpen())
	{
		g_modal.store(nullptr);
		ReleaseWindow(menu);
	}

	return true;
}

void ReportBuild(int added)
{
	if (added == kAppendFaulted)
	{
		sprintf_s(g_status, "adding the mod's page faulted, the game's own menu is unchanged");
		LOG("TrainingMenu: %s", g_status);
		return;
	}

	sprintf_s(g_status, "%d item(s) on the mod's page of the training menu", added);

	if (g_builds.load() == 1)
		LOG("TrainingMenu: the game built its menu, %s", g_status);
}

int __fastcall HookedBuild(void* menu, void* unused)
{
	g_owner.store(nullptr);

	const int built = g_buildHook.Original()(menu, unused);

	g_builds.fetch_add(1);
	CloseModal(nullptr);
	g_extra = {};

	if (built == 0)
		return built;

	ReportBuild(AppendPageGuarded(menu));

	if (!g_extra.ready)
		return built;

	g_memory.restorePending = true;
	g_owner.store(menu);
	return built;
}

void __fastcall HookedUpdate(void* menu, void* unused)
{
	EXPECT_THREAD(ThreadRole::Role_Game);

	const DWORD now = GetTickCount();

	if (now - g_lastUpdate.exchange(now) > kActiveMs)
		CloseModal(menu);

	g_menu = menu;

	if (!HoldForModal(menu))
	{
		ClampCursor(menu);
		g_client->BeforeUpdate();
		g_updateHook.Original()(menu, unused);
		g_client->AfterUpdate();

		if (Field<int>(menu, GameOffsets::kTrainingMenuPage) != kLastSlot)
			ShowExtra(menu, false);

		RememberPage(menu);
	}

	g_menu = nullptr;
}

int __fastcall HookedOpenPicker(void* menu, void* unused, int id, int flag)
{
	if (g_client->OnOpenPicker(id))
		return 1;

	return g_pickerHook.Original()(menu, unused, id, flag);
}

int __fastcall HookedTurnPage(void* menu, void* unused, int delta)
{
	if (!Owns(menu) || delta == 0)
		return g_turnHook.Original()(menu, unused, delta);

	const int from = LogicalPage(menu);
	const int to = delta == GameOffsets::kTrainingMenuFirstPageDelta ? 0
		: Wrap(from + (delta > 0 ? 1 : -1), kLogicalPages);

	ShowExtra(menu, to == kExtraPage);

	if (delta != GameOffsets::kTrainingMenuFirstPageDelta)
		Field<int>(menu, GameOffsets::kTrainingMenuPageBase) = Physical(to) - delta;

	const int result = g_turnHook.Original()(menu, unused, delta);
	ClampCursor(menu);
	return result;
}

int __fastcall HookedResetPage(void* menu, void* unused, int page)
{
	const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress()) - GetGameBaseAddress();

	if (!Owns(menu) || page != kLastSlot || caller != GameOffsets::kTrainingMenuResetAllReturn)
		return g_resetHook.Original()(menu, unused, page);

	const bool shown = g_extra.shown;

	ShowExtra(menu, !shown);
	g_resetHook.Original()(menu, unused, page);
	ShowExtra(menu, shown);

	return g_resetHook.Original()(menu, unused, page);
}

void __cdecl AdjustDots(uintptr_t caller, int* arguments)
{
	if (caller - GetGameBaseAddress() != GameOffsets::kMenuPageDotsTrainingReturn || !OwnsLiveMenu())
		return;

	arguments[1] += 1;

	if (g_extra.shown)
		arguments[0] = kExtraPage;
}

__declspec(naked) void HookedDots()
{
	__asm
	{
		push edx
		push ecx
		lea eax, [esp + 12]
		push eax
		push dword ptr [esp + 12]
		call AdjustDots
		add esp, 8
		pop ecx
		pop edx
		jmp dword ptr [g_dotsOriginal]
	}
}

bool HookDots()
{
	void* const target = reinterpret_cast<void*>(CodeSignatures::Address(GameOffsets::kFnMenuPageDots));

	return IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)) &&
		HookManager::CreateAndEnableHook(target, reinterpret_cast<void*>(&HookedDots), &g_dotsOriginal,
			"MenuPageDots");
}

}

bool TrainingMenu::Install(IClient* client)
{
	if (client == nullptr)
		return false;

	g_client = client;

	const bool hooked = g_updateHook.InstallRva(GameOffsets::kFnTrainingMenuUpdate, &HookedUpdate) &&
		g_pickerHook.InstallRva(GameOffsets::kFnTrainingMenuOpenPicker, &HookedOpenPicker) &&
		g_turnHook.InstallRva(GameOffsets::kFnTrainingMenuTurnPage, &HookedTurnPage) &&
		g_resetHook.InstallRva(GameOffsets::kFnTrainingMenuResetPage, &HookedResetPage) &&
		HookDots() &&
		g_buildHook.InstallRva(GameOffsets::kFnTrainingMenuBuild, &HookedBuild);

	if (!hooked)
	{
		LOG("TrainingMenu: %s", g_status);
		return false;
	}

	sprintf_s(g_status, "waiting for the game to build its training menu");
	LOG("TrainingMenu: hooked, %s", g_status);
	return true;
}

void TrainingMenu::OpenModal(IModal* modal)
{
	if (g_menu == nullptr || modal == nullptr)
		return;

	HoldWindow(g_menu);
	g_modal.store(modal);
}

int TrainingMenu::GetValue(int id)
{
	const MenuItem* const item = FindList(g_menu, id);

	return item == nullptr ? kNoValue : item->current;
}

bool TrainingMenu::SetValue(int id, int value)
{
	MenuItem* const item = FindList(g_menu, id);

	if (item == nullptr || value < 0 || value >= ChoiceCount(*item))
		return false;

	item->current = value;
	return true;
}

bool TrainingMenu::IsActive()
{
	const DWORD last = g_lastUpdate.load();

	return last != 0 && GetTickCount() - last < kActiveMs;
}

bool TrainingMenu::IsHeld()
{
	return g_modal.load() != nullptr && IsActive();
}

const char* TrainingMenu::StatusText()
{
	return g_status;
}
