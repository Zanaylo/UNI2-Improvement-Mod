#include "Game/CharaSelectState.h"

#include "Core/utils.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kMaxCharacter = 64;
constexpr uint32_t kMaxStride = 0x4000;

CharaSelectState::Layout g_layout = {};
char g_status[128] = "no state address in the recipe";

uintptr_t BlockBase()
{
	if (g_layout.pointer == 0 || g_layout.stride == 0 || g_layout.stride > kMaxStride)
		return 0;

	uint32_t value = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(g_layout.pointer)), value))
		return 0;

	return static_cast<uintptr_t>(value);
}

}

void CharaSelectState::Describe(const Layout& layout)
{
	g_layout = layout;
}

bool CharaSelectState::IsLive()
{
	return BlockBase() != 0;
}

int CharaSelectState::CharacterOf(int side)
{
	if (side < 0 || side >= kSideCount)
		return kNoCharacter;

	const uintptr_t base = BlockBase();

	if (base == 0)
	{
		strncpy_s(g_status, "the state pointer reads as null", _TRUNCATE);
		return kNoCharacter;
	}

	const uintptr_t at = base + static_cast<uintptr_t>(side) * g_layout.stride + g_layout.field;

	uint32_t value = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(at), value))
	{
		strncpy_s(g_status, "the character field is unreadable", _TRUNCATE);
		return kNoCharacter;
	}

	if (value > kMaxCharacter)
	{
		sprintf_s(g_status, "field +0x%x holds %u, not a character", g_layout.field, value);
		return kNoCharacter;
	}

	sprintf_s(g_status, "1P/2P read from +0x%x of 0x%p", g_layout.field,
		reinterpret_cast<const void*>(base));

	return static_cast<int>(value);
}

bool CharaSelectState::ReadBlock(int side, uint32_t* out, int count)
{
	if (out == nullptr || count <= 0 || side < 0 || side >= kSideCount)
		return false;

	const uintptr_t base = BlockBase();

	if (base == 0)
		return false;

	const uintptr_t at = base + static_cast<uintptr_t>(side) * g_layout.stride;

	for (int i = 0; i < count; ++i)
	{
		if (!TryReadDword(reinterpret_cast<const void*>(at + static_cast<uintptr_t>(i) * 4),
			out[i]))
		{
			return false;
		}
	}

	return true;
}

const char* CharaSelectState::StatusText()
{
	return g_status;
}
