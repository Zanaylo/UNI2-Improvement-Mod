#include "Game/Engine/GameDraw.h"

#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Game/Engine/GameOffsets.h"

namespace {

typedef void(__fastcall* TextFn)(void* font, void* unused, int zero, int x, int y, const char* text,
	uint32_t colour, int layer);

uintptr_t Function(uintptr_t rva)
{
	const uintptr_t address = CodeSignatures::Address(rva);

	return IsAddressInGameModule(address) ? address : 0;
}

uint8_t* Font(int requested)
{
	uint32_t index = static_cast<uint32_t>(requested);
	uint32_t font = 0;

	if (requested == GameDraw::kCurrentFont &&
		!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kFontIndex)), index))
		return nullptr;

	if (index >= GameOffsets::kFontCount)
		return nullptr;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kFontTable) + index * 4), font) ||
		font == 0)
		return nullptr;

	uint8_t loaded = 0;

	if (!TryReadMemory(&loaded, reinterpret_cast<const void*>(font + 4), sizeof(loaded)) || loaded == 0)
		return nullptr;

	return reinterpret_cast<uint8_t*>(font);
}

int& Field(uint8_t* font, uintptr_t offset)
{
	return *reinterpret_cast<int*>(font + offset);
}

uintptr_t TextFunction(GameDraw::Align align)
{
	switch (align)
	{
	case GameDraw::Align_Centre:
		return Function(GameOffsets::kFnTextCentre);
	case GameDraw::Align_Right:
		return Function(GameOffsets::kFnTextRight);
	default:
		return Function(GameOffsets::kFnTextLeft);
	}
}

void QueueFill(uintptr_t fill, int x, int y, int width, int height, uint32_t colour, int layer)
{
	__asm
	{
		push 1
		push layer
		push 0
		push colour
		push colour
		push colour
		push colour
		push height
		push width
		push y
		mov edx, x
		xor ecx, ecx
		call fill
		add esp, 0x28
	}
}

}

bool GameDraw::IsReady()
{
	return Font(kCurrentFont) != nullptr && Function(GameOffsets::kFnQueueFill) != 0;
}

bool GameDraw::Fill(int x, int y, int width, int height, uint32_t colour, int layer)
{
	const uintptr_t fill = Function(GameOffsets::kFnQueueFill);

	if (fill == 0 || width <= 0 || height <= 0)
		return false;

	QueueFill(fill, x, y, width, height, colour, layer);
	return true;
}

bool GameDraw::Frame(int x, int y, int width, int height, uint32_t colour, int layer)
{
	return Fill(x, y, width, 1, colour, layer) && Fill(x, y + height - 1, width, 1, colour, layer) &&
		Fill(x, y, 1, height, colour, layer) && Fill(x + width - 1, y, 1, height, colour, layer);
}

bool GameDraw::Text(Align align, int x, int y, const char* text, uint32_t colour, int layer, Style style)
{
	uint8_t* const font = Font(style.font);
	const uintptr_t draw = TextFunction(align);

	if (font == nullptr || draw == 0 || text == nullptr)
		return false;

	const int scaleX = Field(font, GameOffsets::kFontScaleX);
	const int scaleY = Field(font, GameOffsets::kFontScaleY);

	Field(font, GameOffsets::kFontScaleX) = style.scale;
	Field(font, GameOffsets::kFontScaleY) = style.scale;

	reinterpret_cast<TextFn>(draw)(font, nullptr, 0, x, y, text, colour, layer);

	Field(font, GameOffsets::kFontScaleX) = scaleX;
	Field(font, GameOffsets::kFontScaleY) = scaleY;
	return true;
}

int GameDraw::LineHeight(Style style)
{
	uint8_t* const font = Font(style.font);

	if (font == nullptr)
		return 0;

	return Field(font, GameOffsets::kFontLineHeight) * style.scale / GameOffsets::kFontFullScale;
}
