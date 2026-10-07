#include "Game/Menus/ColourWindow.h"

#include "Core/logger.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Menus/ColourIconEntries.h"
#include "Game/Menus/ColourNameTable.h"
#include "Game/Menus/ColourPicker.h"
#include "Game/Menus/ColourPreview.h"
#include "Hooks/GameHook.h"
#include "Palette/ColourSlotStep.h"
#include "Palette/PaletteFile.h"

#include <cstdio>
#include <cstring>

namespace {

typedef void(__fastcall* WindowDrawFn)(void* window);
typedef int(__thiscall* MenuTextFn)(void* font, int flags, int x, int y, const char* text, int width, int height,
	uint32_t colour, int layer);

constexpr int kNameLength = 24;
constexpr int kRgb = 3;
constexpr int kRgbaStride = 4;

struct Swatches
{
	uint8_t* entry;
	uint8_t saved[GameOffsets::kColourNameSwatches][kRgb];
	bool swapped;
};

GameHook<WindowDrawFn> g_drawHook("ColourWindowDraw");
GameHook<MenuTextFn> g_textHook("ColourWindowText");

char g_label[64] = {};
bool g_armed = false;

char g_status[96] = "not installed";

int& Field(void* window, uintptr_t offset)
{
	return *reinterpret_cast<int*>(static_cast<uint8_t*>(window) + offset);
}

void Label(const ColourPicker::Shown& shown)
{
	char name[kNameLength + 1] = {};
	strncpy_s(name, shown.file, _TRUNCATE);

	char* const extension = strrchr(name, '.');

	if (extension != nullptr && _stricmp(extension, PaletteFile::kExtension) == 0)
		*extension = '\0';

	sprintf_s(g_label, "EX%02d  %s", shown.extended, name);
}

Swatches PaintSwatches(int side, int chara, int colour)
{
	Swatches swatches = { ColourNameTable::Entry(chara, colour), {}, false };
	const uint8_t* const colours = ColourPreview::Colours(side);

	int entries[GameOffsets::kColourNameSwatches] = {};

	if (swatches.entry == nullptr || colours == nullptr ||
		!ColourIconEntries::Find(chara, entries))
	{
		return swatches;
	}

	for (int swatch = 0; swatch < GameOffsets::kColourNameSwatches; ++swatch)
	{
		uint8_t* const target = ColourNameTable::Swatch(swatches.entry, swatch);

		memcpy(swatches.saved[swatch], target, kRgb);
		memcpy(target, colours + entries[swatch] * kRgbaStride, kRgb);
	}

	swatches.swapped = true;
	return swatches;
}

void RestoreSwatches(const Swatches& swatches)
{
	if (!swatches.swapped)
		return;

	for (int swatch = 0; swatch < GameOffsets::kColourNameSwatches; ++swatch)
		memcpy(ColourNameTable::Swatch(swatches.entry, swatch), swatches.saved[swatch], kRgb);
}

void DrawExtended(void* window, const ColourPicker::Shown& shown, int gameCount)
{
	int& index = Field(window, GameOffsets::kColourWindowIndex);
	const int gameIndex = index;

	index = gameCount - 1 + shown.extended;

	const Swatches swatches = PaintSwatches(Field(window, GameOffsets::kColourWindowSide), shown.chara,
		Field(window, GameOffsets::kColourWindowColour));

	Label(shown);
	g_armed = true;

	g_drawHook.Original()(window);

	g_armed = false;
	RestoreSwatches(swatches);

	index = gameIndex;
}

void __fastcall HookedDraw(void* window)
{
	ColourPicker::Shown shown = {};

	if (window == nullptr || !ColourPicker::Describe(Field(window, GameOffsets::kColourWindowSide), shown) ||
		shown.chara != Field(window, GameOffsets::kColourWindowChara) || shown.count <= 0)
	{
		g_drawHook.Original()(window);
		return;
	}

	int& count = Field(window, GameOffsets::kColourWindowCount);
	const int gameCount = count;

	count = gameCount + shown.count;

	if (shown.extended == ColourSlotStep::kNoExtended)
		g_drawHook.Original()(window);
	else
		DrawExtended(window, shown, gameCount);

	count = gameCount;
}

int __fastcall HookedText(void* font, void* unused, int flags, int x, int y, const char* text, int width, int height,
	uint32_t colour, int layer)
{
	if (g_armed)
	{
		g_armed = false;
		text = g_label;
	}

	return g_textHook.Original()(font, flags, x, y, text, width, height, colour, layer);
}

}

bool ColourWindow::Install()
{
	if (g_drawHook.IsLive())
		return true;

	if (!g_textHook.InstallRva(GameOffsets::kFnMenuText, &HookedText) ||
		!g_drawHook.InstallRva(GameOffsets::kFnColourWindowDraw, &HookedDraw))
	{
		strncpy_s(g_status, "the colour window is not where this game version expects it", _TRUNCATE);
		LOG("colour window: %s", g_status);
		return false;
	}

	strncpy_s(g_status, "on", _TRUNCATE);
	LOG("colour window: hooked, extended slots are drawn in the game's own window");
	return true;
}

const char* ColourWindow::StatusText()
{
	return g_status;
}
