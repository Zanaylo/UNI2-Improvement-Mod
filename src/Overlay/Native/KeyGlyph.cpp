#include "Overlay/Native/KeyGlyph.h"

#include "Core/Input/PadInput.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Menus/MenuKeys.h"
#include "Overlay/Native/GameArt.h"

#include <Windows.h>

namespace {

struct KeyCell
{
	int key;
	int column;
	int row;
};

const KeyCell kSpecialKeys[] = {
	{ VK_DIVIDE, 0, 5 }, { VK_MULTIPLY, 1, 5 }, { VK_ADD, 2, 5 }, { VK_SUBTRACT, 3, 5 },
	{ VK_DECIMAL, 4, 5 },
	{ VK_INSERT, 0, 6 }, { VK_DELETE, 1, 6 }, { VK_HOME, 2, 6 }, { VK_END, 3, 6 }, { VK_PRIOR, 4, 6 },
	{ VK_NEXT, 5, 6 }, { VK_TAB, 6, 6 }, { VK_SPACE, 7, 6 }, { VK_RETURN, 8, 6 }, { VK_BACK, 9, 6 },
	{ VK_ESCAPE, 10, 6 },
	{ VK_OEM_MINUS, 0, 7 }, { VK_OEM_3, 1, 7 }, { VK_OEM_4, 2, 7 }, { VK_OEM_PLUS, 3, 7 },
	{ VK_OEM_1, 4, 7 }, { VK_OEM_7, 5, 7 }, { VK_OEM_6, 6, 7 }, { VK_OEM_COMMA, 7, 7 },
	{ VK_OEM_PERIOD, 8, 7 }, { VK_OEM_2, 9, 7 }, { VK_OEM_102, 10, 7 }, { VK_OEM_5, 11, 7 },
	{ VK_UP, 0, 8 }, { VK_DOWN, 1, 8 }, { VK_LEFT, 2, 8 }, { VK_RIGHT, 3, 8 },
	{ VK_SHIFT, 4, 8 }, { VK_LSHIFT, 4, 8 }, { VK_RSHIFT, 5, 8 },
	{ VK_CONTROL, 6, 8 }, { VK_LCONTROL, 6, 8 }, { VK_RCONTROL, 7, 8 },
	{ VK_MENU, 8, 8 }, { VK_LMENU, 8, 8 }, { VK_RMENU, 9, 8 },
};

struct PadGlyph
{
	int function;
	GameArt::Sprite sprite;
};

const PadGlyph kPadGlyphs[] = {
	{ GameOffsets::kMenuKeyConfirm, GameArt::kPadA },
	{ GameOffsets::kMenuKeyCancel, GameArt::kPadB },
	{ GameOffsets::kMenuKeyOpenMenu, GameArt::kPadX },
	{ GameOffsets::kMenuKeyNextPage, GameArt::kPadRb },
	{ GameOffsets::kMenuKeyPreviousPage, GameArt::kPadLb },
};

DWORD g_keyboardAt = 1;
DWORD g_padAt = 0;

bool Range(int key, int first, int last, int row, GameArt::Sprite& out)
{
	if (key < first || key > last)
		return false;

	out = GameArt::Keycap(key - first, row);
	return true;
}

bool KeycapFor(int key, GameArt::Sprite& out)
{
	if (Range(key, 'A', 'M', 0, out) || Range(key, 'N', 'Z', 1, out) || Range(key, '1', '9', 2, out) ||
		Range(key, VK_F1, VK_F12, 3, out) || Range(key, VK_NUMPAD1, VK_NUMPAD9, 4, out))
	{
		return true;
	}

	if (key == '0' || key == VK_NUMPAD0)
	{
		out = GameArt::Keycap(9, key == '0' ? 2 : 4);
		return true;
	}

	for (const KeyCell& cell : kSpecialKeys)
	{
		if (cell.key != key)
			continue;

		out = GameArt::Keycap(cell.column, cell.row);
		return true;
	}

	return false;
}

bool PadFor(int function, GameArt::Sprite& out)
{
	for (const PadGlyph& glyph : kPadGlyphs)
	{
		if (glyph.function != function)
			continue;

		out = glyph.sprite;
		return true;
	}

	return false;
}

bool UsingPad()
{
	return PadInput::IsConnected() && g_padAt > g_keyboardAt;
}

bool SpriteFor(int function, GameArt::Sprite& out)
{
	if (UsingPad())
		return PadFor(function, out);

	return KeycapFor(MenuKeys::KeyboardKey(function), out);
}

float WidthOf(const GameArt::Sprite& sprite, float size)
{
	return size * sprite.width / sprite.height;
}

}

void KeyGlyph::Observe()
{
	const DWORD now = GetTickCount();

	if (PadInput::PollPressedButton() != PadInput::kNone)
		g_padAt = now;

	for (int function = 0; function < GameOffsets::kMenuKeyCount; ++function)
	{
		const int key = MenuKeys::KeyboardKey(function);

		if (key != MenuKeys::kUnbound && (GetAsyncKeyState(key) & 0x8000) != 0)
			g_keyboardAt = now;
	}
}

float KeyGlyph::Width(int function, float size)
{
	GameArt::Sprite sprite = {};

	return SpriteFor(function, sprite) ? WidthOf(sprite, size) : 0.0f;
}

float KeyGlyph::Draw(int function, float x, float y, float size)
{
	GameArt::Sprite sprite = {};

	if (!SpriteFor(function, sprite))
		return 0.0f;

	const float width = WidthOf(sprite, size);
	GameArt::Draw(sprite, x, y, width, size);
	return width;
}
