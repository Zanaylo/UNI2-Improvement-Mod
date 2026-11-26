#include "Palette/PaletteOwnerProbe.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/MemoryMap.h"
#include "Hooks/GameHook.h"
#include "Palette/PalettePaint.h"
#include "Palette/PaletteSeat.h"
#include "Game/Engine/CodeSignatures.h"

#include <d3d9.h>

#include <cstring>

namespace {


constexpr uintptr_t kDrawOverride = 0x24;
constexpr uintptr_t kDrawOwner = 0x48;
constexpr uintptr_t kDrawPair = 0x2c;
constexpr uintptr_t kDrawOdd = 0x20;

constexpr uintptr_t kOwnerPalette = 0x15054;

constexpr int kMaxRows = 32;

typedef void(__fastcall* CharaDraw)(void* self, void* unused);

GameHook<CharaDraw> g_charaDrawHook("chara draw (palette owner probe)");

PaletteOwnerProbe::Row g_rows[kMaxRows] = {};
int g_count = 0;

volatile long g_enabled = 0;
uintptr_t g_lastBadOverride = 0;
uintptr_t g_lastBadHolder = 0;

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

	LOG("palette owner: owner 0x%08x texture 0x%08x override 0x%08x side %d object %d depth %d",
		static_cast<unsigned>(owner), static_cast<unsigned>(texture),
		static_cast<unsigned>(override), row, chara, depth);
}

bool LooksLikePointer(uintptr_t value)
{
	return value >= 0x10000 && (value & 3) == 0;
}

bool IsValidPaletteHolder(uintptr_t holder)
{
	if (!LooksLikePointer(holder))
		return false;

	bool valid = false;

	__try
	{
		const uintptr_t wrapper = *reinterpret_cast<const uintptr_t*>(holder);

		if (LooksLikePointer(wrapper))
		{
			const uintptr_t texture = *reinterpret_cast<const uintptr_t*>(
				wrapper + GameOffsets::kPaletteWrapperTexture);

			if (LooksLikePointer(texture))
			{
				const uintptr_t vtable = *reinterpret_cast<const uintptr_t*>(texture);

				if (LooksLikePointer(vtable))
				{
					IDirect3DBaseTexture9* const base = reinterpret_cast<IDirect3DBaseTexture9*>(texture);
					valid = base->GetType() == D3DRTYPE_TEXTURE;
				}
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		valid = false;
	}

	return valid;
}

bool HasDanglingWrapper(uintptr_t holder)
{
	if (!LooksLikePointer(holder))
		return true;

	uintptr_t wrapper = 0;

	__try
	{
		wrapper = *reinterpret_cast<const uintptr_t*>(holder);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return true;
	}

	if (wrapper == 0)
		return false;

	return !IsValidPaletteHolder(holder);
}

struct DrawFields
{
	uintptr_t override;
	uintptr_t owner;
	uintptr_t ownerHolder;
	uintptr_t texture;
	int row;
};

bool ReadDrawFields(void* self, DrawFields& out)
{
	if (self == nullptr)
		return false;

	const uintptr_t draw = reinterpret_cast<uintptr_t>(self);

	if (!LooksLikePointer(draw))
		return false;

	bool ok = false;

	__try
	{
		const uintptr_t override = *reinterpret_cast<const uintptr_t*>(draw + kDrawOverride);
		const uintptr_t owner = *reinterpret_cast<const uintptr_t*>(draw + kDrawOwner);
		const int pair = *reinterpret_cast<const int*>(draw + kDrawPair);
		const int odd = *reinterpret_cast<const int*>(draw + kDrawOdd);

		uintptr_t ownerHolder = 0;

		if (LooksLikePointer(owner))
			ownerHolder = *reinterpret_cast<const uintptr_t*>(owner + kOwnerPalette);

		const uintptr_t holder = override != 0 ? override : ownerHolder;
		uintptr_t texture = 0;

		if (LooksLikePointer(holder))
			texture = *reinterpret_cast<const uintptr_t*>(holder);

		out.override = override;
		out.owner = owner;
		out.ownerHolder = ownerHolder;
		out.texture = texture;
		out.row = pair * 2 + (odd != 0 ? 1 : 0);
		ok = true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ok = false;
	}

	return ok;
}

bool SanitizeDrawPalette(void* self, DrawFields& fields)
{
	if (fields.override != 0)
	{
		if (IsValidPaletteHolder(fields.override))
			return true;

		if (IsValidPaletteHolder(fields.ownerHolder))
		{
			const uintptr_t bad = fields.override;
			*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(self) + kDrawOverride) = 0;
			fields.override = 0;
			TryReadMemory(&fields.texture, reinterpret_cast<const void*>(fields.ownerHolder),
				sizeof(fields.texture));

			if (bad != g_lastBadOverride)
			{
				g_lastBadOverride = bad;
				LOG_WARN("palette owner: cleared dangling override 0x%08x for owner 0x%08x (side %d)",
					static_cast<unsigned>(bad), static_cast<unsigned>(fields.owner), fields.row);
			}

			return true;
		}

		if (fields.override != g_lastBadOverride)
		{
			g_lastBadOverride = fields.override;
			LOG_WARN("palette owner: skipped draw with dangling override 0x%08x and no valid owner palette",
				static_cast<unsigned>(fields.override));
		}

		return false;
	}

	if (fields.ownerHolder != 0 && HasDanglingWrapper(fields.ownerHolder))
	{
		if (fields.ownerHolder != g_lastBadHolder)
		{
			g_lastBadHolder = fields.ownerHolder;
			LOG_WARN("palette owner: skipped draw with dangling owner holder 0x%08x (side %d)",
				static_cast<unsigned>(fields.ownerHolder), fields.row);
		}

		return false;
	}

	return true;
}

void __fastcall Detour(void* self, void* unused)
{
	DrawFields fields = {};

	if (ReadDrawFields(self, fields))
	{
		if (!SanitizeDrawPalette(self, fields))
			return;

		PaletteSeat::OnDraw(fields.owner, fields.texture, fields.row);

		PalettePaint::OnDraw();

		if (g_enabled != 0)
		{
			int depth = 0;
			const int chara = CharaFromStack(depth);

			Record(fields.owner, fields.texture, fields.override, fields.row, chara, depth);
		}
	}

	g_charaDrawHook.Original()(self, unused);
}

}

bool PaletteOwnerProbe::Install()
{
	void* const target = reinterpret_cast<void*>(CodeSignatures::Address(GameOffsets::kFnCharaDraw));
	if (target == nullptr)
		return false;

	return g_charaDrawHook.Install(target, &Detour);
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
