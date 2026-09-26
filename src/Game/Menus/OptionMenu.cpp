#include "Game/Menus/OptionMenu.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameDraw.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Menus/MenuInput.h"
#include "Hooks/GameHook.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

typedef void(__fastcall* AllocateRowsFn)(void* self, void* unused, int count);
typedef int(__fastcall* BuildFn)(void* self, void* unused);
typedef int(__fastcall* UpdateFn)(void* self, void* unused);
typedef int(__fastcall* DrawFn)(void* self, void* unused);
typedef void(__fastcall* DefaultsFn)(void* self, void* unused);
typedef uint32_t(__fastcall* ColourFn)(void* self, void* unused, int row, int value, uint32_t colour);
typedef void(__fastcall* AddRowFn)(void* self, void* unused, int row, const char* word, int unused2,
	const char* info);
typedef void(__fastcall* AddChoiceFn)(void* self, void* unused, int row, const char* text, uint32_t colour);

struct GameVector
{
	void** begin;
	void** end;
	void** capacity;
};

struct SavedRow
{
	char word[GameOffsets::kOptionRowWordSize];
	char info[GameOffsets::kOptionRowInfoSize];
};

GameHook<AllocateRowsFn> g_allocateHook("OptionRowsAllocate");
GameHook<BuildFn> g_buildHook("OptionDisplayBuild");
GameHook<UpdateFn> g_updateHook("OptionDisplayUpdate");
GameHook<DefaultsFn> g_defaultsHook("OptionDisplayDefaults");
GameHook<ColourFn> g_colourHook("OptionDisplayColour");
GameHook<DrawFn> g_drawHook("OptionDisplayDraw");

constexpr int kActions = 3;
const char* const kConfirmWord = "<GR_NM_Confirm>";
const char* const kConfirmInfo = "Apply the settings above.";

constexpr int kTitleLeft = 512;
constexpr int kTitleWidth = 256;
constexpr int kTitleInset = 2;
constexpr int kTitleBarHeight = 32;
constexpr int kTitleOffset = 0x10;
constexpr int kTitleFont = 1;
constexpr int kTitleScale = 100;
constexpr int kTitleCentreX = 640;
constexpr uint32_t kTitleBar = 0xFFE6E6E6;
constexpr uint32_t kTitleText = 0xFF141414;

AddRowFn g_addRow = nullptr;
AddChoiceFn g_addChoice = nullptr;
UpdateFn g_baseUpdate = nullptr;

OptionMenu::IClient* g_client = nullptr;
bool g_buildingDisplay = false;
void* g_tab = nullptr;

char g_status[160] = "the options screen is not where this game version expects it";

template <typename T>
T& Field(void* base, uintptr_t offset)
{
	return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

int& RowCount(void* component)
{
	return Field<int>(component, GameOffsets::kOptionRowCount);
}

int& Cursor(void* component)
{
	return Field<int>(component, GameOffsets::kOptionCursor);
}

uint8_t* Row(void* component, int index)
{
	uint8_t* const rows = Field<uint8_t*>(component, GameOffsets::kOptionRows);

	if (rows == nullptr || index < 0 || index >= RowCount(component))
		return nullptr;

	return rows + static_cast<size_t>(index) * GameOffsets::kOptionRowSize;
}

int& RowValue(uint8_t* row)
{
	return Field<int>(row, GameOffsets::kOptionRowValue);
}

int ClientRows()
{
	return (std::min)(g_client->RowCount(), OptionMenu::kMaxRows - kActions);
}

int EntryRow()
{
	return GameOffsets::kOptionDisplayRows;
}

int AllocatedRows()
{
	return (std::max)(GameOffsets::kOptionDisplayRows + 1, ClientRows() + kActions);
}

int SettingY(int index)
{
	return index * GameOffsets::kOptionRowSpacing;
}

int ActionY(int settings, int action)
{
	return SettingY(settings) + GameOffsets::kOptionActionGap + action * GameOffsets::kOptionRowSpacing;
}

void Place(uint8_t* row, int y)
{
	Field<int>(row, GameOffsets::kOptionRowX) = 0;
	Field<int>(row, GameOffsets::kOptionRowY) = y;
}

void ClearChoices(uint8_t* row)
{
	GameVector& choices = Field<GameVector>(row, GameOffsets::kOptionRowChoices);
	GameVector& colours = Field<GameVector>(row, GameOffsets::kOptionRowColours);

	choices.end = choices.begin;
	colours.end = colours.begin;
}

void Fill(void* component, int index, const char* word, const char* info, int y)
{
	uint8_t* const row = Row(component, index);

	if (row == nullptr)
		return;

	ClearChoices(row);
	g_addRow(component, nullptr, index, word, 0, info);
	Field<uint8_t>(row, GameOffsets::kOptionRowLocked) = 0;
	RowValue(row) = 0;
	Place(row, y);
}

void Save(void* component, int index, SavedRow& out)
{
	const uint8_t* const row = Row(component, index);

	out = SavedRow();

	if (row == nullptr)
		return;

	strncpy_s(out.word, reinterpret_cast<const char*>(row + GameOffsets::kOptionRowWord), _TRUNCATE);
	strncpy_s(out.info, reinterpret_cast<const char*>(row + GameOffsets::kOptionRowInfo), _TRUNCATE);
}

void FillClientRow(void* component, int index)
{
	const OptionMenu::RowSpec spec = g_client->Row(index);

	Fill(component, index, spec.word, spec.info, SettingY(index));

	for (int choice = 0; choice < spec.choiceCount; ++choice)
		g_addChoice(component, nullptr, index, spec.choices[choice], 0);

	uint8_t* const row = Row(component, index);

	if (row == nullptr || spec.choiceCount <= 0)
		return;

	RowValue(row) = (std::min)((std::max)(g_client->Current(index), 0), spec.choiceCount - 1);
}

void EnterTab(void* component)
{
	SavedRow reset;
	SavedRow back;

	Save(component, GameOffsets::kOptionDisplayResetRow, reset);
	Save(component, GameOffsets::kOptionDisplayReturnRow, back);

	const int rows = ClientRows();

	RowCount(component) = rows + kActions;

	for (int index = 0; index < rows; ++index)
		FillClientRow(component, index);

	Fill(component, rows, kConfirmWord, kConfirmInfo, ActionY(rows, 0));
	Fill(component, rows + 1, reset.word, reset.info, ActionY(rows, 1));
	Fill(component, rows + 2, back.word, back.info, ActionY(rows, 2));

	char* const title = &Field<char>(component, GameOffsets::kOptionTitle);
	strncpy_s(title, GameOffsets::kOptionTitleSize, g_client->Title(), _TRUNCATE);
	Field<char>(component, GameOffsets::kOptionSubtitle) = '\0';

	Cursor(component) = 0;
	g_tab = component;

	sprintf_s(g_status, "the mod's tab is shown");
	LOG("OptionMenu: the mod's display tab is shown, %d row(s)", rows);
}

void ApplyAll(void* component)
{
	const int rows = ClientRows();

	for (int index = 0; index < rows; ++index)
	{
		uint8_t* const row = Row(component, index);

		if (row != nullptr)
			g_client->Apply(index, RowValue(row));
	}
}

bool ConfirmPressed()
{
	MenuInput::State input;

	return MenuInput::ReadShared(input) && input.confirm;
}

void __fastcall HookedAllocateRows(void* self, void* unused, int count)
{
	g_allocateHook.Original()(self, unused, g_buildingDisplay ? AllocatedRows() : count);
}

int __fastcall HookedBuild(void* self, void* unused)
{
	if (self == g_tab)
		g_tab = nullptr;

	g_buildingDisplay = true;
	const int result = g_buildHook.Original()(self, unused);
	g_buildingDisplay = false;

	if (RowCount(self) < EntryRow() + 1)
		return result;

	const int settings = GameOffsets::kOptionDisplayResetRow;

	RowCount(self) = EntryRow() + 1;
	Fill(self, EntryRow(), g_client->EntryWord(), g_client->EntryInfo(), ActionY(settings, 2));

	for (int action = 0; action < 2; ++action)
	{
		uint8_t* const row = Row(self, settings + action);

		if (row != nullptr)
			Place(row, ActionY(settings, action));
	}

	sprintf_s(g_status, "the display page carries the mod's entry");
	return result;
}

int UpdateTab(void* self)
{
	const bool confirm = ConfirmPressed();
	const int result = g_baseUpdate(self, nullptr);

	if (result == -1)
	{
		g_tab = nullptr;
		sprintf_s(g_status, "the display page carries the mod's entry");
		return result;
	}

	const int cursor = Cursor(self);

	if (confirm && cursor >= 0 && cursor <= ClientRows() + 1)
		ApplyAll(self);

	return result;
}

int __fastcall HookedUpdate(void* self, void* unused)
{
	if (self == g_tab)
		return UpdateTab(self);

	const int result = g_updateHook.Original()(self, unused);

	if (result != -1 && Cursor(self) == EntryRow() && ConfirmPressed())
		EnterTab(self);

	return result;
}

void __fastcall HookedDefaults(void* self, void* unused)
{
	if (self != g_tab)
	{
		g_defaultsHook.Original()(self, unused);
		return;
	}

	const int rows = ClientRows();

	for (int index = 0; index < rows; ++index)
	{
		uint8_t* const row = Row(self, index);

		if (row != nullptr)
			RowValue(row) = g_client->Default(index);
	}
}

uint32_t __fastcall HookedColour(void* self, void* unused, int row, int value, uint32_t colour)
{
	if (self != g_tab)
		return g_colourHook.Original()(self, unused, row, value, colour);

	if (row < 0 || row >= ClientRows() || value == g_client->Default(row))
		return colour;

	return GameOffsets::kOptionChangedColour;
}

void DrawTitle(void* self)
{
	const int layer = Field<int>(self, GameOffsets::kOptionLayer) + 1;
	const float open = Field<float>(self, GameOffsets::kOptionOpenScale);
	const int height = Field<int>(self, GameOffsets::kOptionPanelHeight);
	const int top = Field<int>(self, GameOffsets::kOptionPanelCentreY) - static_cast<int>(height * open) / 2 +
		kTitleOffset;
	const GameDraw::Style style = { kTitleFont, kTitleScale };
	const int textTop = top + (kTitleBarHeight - GameDraw::LineHeight(style)) / 2;

	GameDraw::Text(GameDraw::Align_Centre, kTitleCentreX, textTop, g_client->Title(), kTitleText, layer, style);
	GameDraw::Fill(kTitleLeft, top + kTitleInset, kTitleWidth, kTitleBarHeight - kTitleInset * 2, kTitleBar, layer);
}

int __fastcall HookedDraw(void* self, void* unused)
{
	const int result = g_drawHook.Original()(self, unused);

	if (self == g_tab && GameDraw::IsReady())
		DrawTitle(self);

	return result;
}

template <typename Fn>
Fn Resolve(uintptr_t rva)
{
	const uintptr_t address = CodeSignatures::Address(rva);

	return IsAddressInGameModule(address) ? reinterpret_cast<Fn>(address) : nullptr;
}

}

bool OptionMenu::Install(IClient* client)
{
	if (client == nullptr)
		return false;

	g_client = client;
	g_addRow = Resolve<AddRowFn>(GameOffsets::kFnOptionRowAdd);
	g_addChoice = Resolve<AddChoiceFn>(GameOffsets::kFnOptionChoiceAdd);
	g_baseUpdate = Resolve<UpdateFn>(GameOffsets::kFnOptionBaseUpdate);

	if (g_addRow == nullptr || g_addChoice == nullptr || g_baseUpdate == nullptr)
	{
		LOG("OptionMenu: %s", g_status);
		return false;
	}

	const bool hooked = g_allocateHook.InstallRva(GameOffsets::kFnOptionRowsAllocate, &HookedAllocateRows) &&
		g_updateHook.InstallRva(GameOffsets::kFnOptionDisplayUpdate, &HookedUpdate) &&
		g_defaultsHook.InstallRva(GameOffsets::kFnOptionDisplayDefaults, &HookedDefaults) &&
		g_colourHook.InstallRva(GameOffsets::kFnOptionDisplayColour, &HookedColour) &&
		g_drawHook.InstallRva(GameOffsets::kFnOptionDisplayDraw, &HookedDraw) &&
		g_buildHook.InstallRva(GameOffsets::kFnOptionDisplayBuild, &HookedBuild);

	if (!hooked)
	{
		LOG("OptionMenu: %s", g_status);
		return false;
	}

	sprintf_s(g_status, "waiting for the display page");
	LOG("OptionMenu: hooked, %s", g_status);
	return true;
}

const char* OptionMenu::StatusText()
{
	return g_status;
}
