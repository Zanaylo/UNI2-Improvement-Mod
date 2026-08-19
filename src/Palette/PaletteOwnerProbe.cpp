#include "Palette/PaletteOwnerProbe.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/MemoryMap.h"
#include "Hooks/HookManager.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteSeat.h"

#include <cstring>

namespace {

constexpr uintptr_t kCharaDrawRva = 0xbdf50;

constexpr uintptr_t kDrawOverride = 0x24;
constexpr uintptr_t kDrawOwner = 0x48;
constexpr uintptr_t kDrawPair = 0x2c;
constexpr uintptr_t kDrawOdd = 0x20;

constexpr uintptr_t kOwnerPalette = 0x15050;

constexpr int kMaxRows = 32;

typedef void(__fastcall* CharaDraw)(void* self, void* unused);

CharaDraw g_original = nullptr;

PaletteOwnerProbe::Row g_rows[kMaxRows] = {};
int g_count = 0;

volatile long g_enabled = 0;

bool ReadPointer(uintptr_t address, uintptr_t& out)
{
	return address != 0
		&& TryReadMemory(&out, reinterpret_cast<const void*>(address), sizeof(out));
}

bool ReadInt(uintptr_t address, int& out)
{
	return address != 0
		&& TryReadMemory(&out, reinterpret_cast<const void*>(address), sizeof(out));
}

int CharaFromStack(int& outDepth)
{
	outDepth = 0;

	MemoryMap::CharaStackView stack = {};
	if (!MemoryMap::ReadCharaStack(stack) || stack.depth <= 0)
		return -1;

	outDepth = stack.depth;

	void* const top = stack.entries[0];
	if (top == nullptr || !MemoryMap::IsPlausibleCharaData(top))
		return -1;

	int id = 0;
	if (!ReadInt(reinterpret_cast<uintptr_t>(top) + GameOffsets::kCharaObjectId, id))
		return -1;

	return id;
}

void Record(uintptr_t owner, uintptr_t texture, uintptr_t override, int row, int chara, int depth)
{
	for (int i = 0; i < g_count; ++i)
	{
		PaletteOwnerProbe::Row& seen = g_rows[i];

		if (seen.owner != owner || seen.texture != texture || seen.row != row)
			continue;

		++seen.draws;

		if (seen.charaFromStack < 0 && chara >= 0)
		{
			seen.charaFromStack = chara;
			seen.stackDepth = depth;
		}

		return;
	}

	if (g_count >= kMaxRows)
		return;

	PaletteOwnerProbe::Row& fresh = g_rows[g_count++];

	fresh.owner = owner;
	fresh.texture = texture;
	fresh.override = override;
	fresh.row = row;
	fresh.charaFromStack = chara;
	fresh.stackDepth = depth;
	fresh.draws = 1;
}

void __fastcall Detour(void* self, void* unused)
{
	if (self != nullptr)
	{
		const uintptr_t draw = reinterpret_cast<uintptr_t>(self);

		uintptr_t override = 0;
		uintptr_t owner = 0;
		int pair = 0;
		int odd = 0;

		if (ReadPointer(draw + kDrawOverride, override)
			&& ReadPointer(draw + kDrawOwner, owner)
			&& ReadInt(draw + kDrawPair, pair)
			&& ReadInt(draw + kDrawOdd, odd))
		{
			uintptr_t holder = override;

			if (holder == 0 && owner != 0)
				ReadPointer(owner + kOwnerPalette, holder);

			uintptr_t texture = 0;
			if (holder != 0)
				ReadPointer(holder, texture);

			const int side = pair * 2 + (odd != 0 ? 1 : 0);

			PaletteSeat::OnDraw(owner, texture, side);

			PalettePaint::OnDraw();

			if (g_enabled != 0)
			{
				int depth = 0;
				const int chara = CharaFromStack(depth);

				Record(owner, texture, override, side, chara, depth);
			}
		}
	}

	g_original(self, unused);
}

}

bool PaletteOwnerProbe::Install()
{
	void* const target = reinterpret_cast<void*>(RvaToAddress(kCharaDrawRva));
	if (target == nullptr)
		return false;

	return HookManager::CreateAndEnableHook(target, &Detour,
		reinterpret_cast<void**>(&g_original), "chara draw (palette owner probe)");
}

void PaletteOwnerProbe::SetEnabled(bool enabled)
{
	g_enabled = enabled ? 1 : 0;
}

bool PaletteOwnerProbe::IsEnabled()
{
	return g_enabled != 0;
}

int PaletteOwnerProbe::GetCount()
{
	return g_count;
}

bool PaletteOwnerProbe::Get(int index, Row& out)
{
	if (index < 0 || index >= g_count)
		return false;

	out = g_rows[index];
	return true;
}

void PaletteOwnerProbe::Reset()
{
	g_count = 0;
	memset(g_rows, 0, sizeof(g_rows));
}
