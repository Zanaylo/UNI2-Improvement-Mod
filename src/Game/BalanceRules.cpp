#include "Game/BalanceRules.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr BalanceRules::Rule kRules[] = {
	{
		"min-damage-floor",
		"Minimum guaranteed damage at its pre-1.10 value",
		"1.10 scaled every minimum guaranteed damage to four fifths. While an older patch is "
		"loaded the mod runs the older arithmetic instead, so EX and Infinite Worth floors pay "
		"exactly what they did up to 1.05.",
		110
	}
};

constexpr int kCount = static_cast<int>(sizeof(kRules) / sizeof(kRules[0]));

typedef int(__fastcall* MinDamageFn)(void* self, void* unused, int damage);

MinDamageFn oMinDamage = nullptr;

int g_version = 0;
bool g_active[kCount] = {};
char g_status[192] = "off";

const uint8_t* Owner(const uint8_t* chara)
{
	if (chara == nullptr)
		return nullptr;

	uint32_t redirect = 0;

	if (TryReadDword(chara + 0x3f8, redirect) && redirect != 0)
		return reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(redirect));

	return chara;
}

int __fastcall HookedMinDamage(void* self, void* unused, int damage)
{
	if (!g_active[0] || self == nullptr)
		return oMinDamage(self, unused, damage);

	const uint8_t* const context = static_cast<const uint8_t*>(self);

	uint32_t base = 0;
	uint32_t charaValue = 0;

	if (!TryReadDword(context + 0x10, base) || !TryReadDword(context + 4, charaValue))
		return oMinDamage(self, unused, damage);

	int floor = static_cast<int>(base);

	if (floor < 1)
		return 0;

	const uint8_t* const chara =
		Owner(reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(charaValue)));

	uint8_t hosei = 0;

	if (chara == nullptr || !TryReadMemory(&hosei, chara + GameOffsets::kCharaHoseiMin, 1))
		return oMinDamage(self, unused, damage);

	if (hosei != 0)
		floor = static_cast<int>(static_cast<unsigned>(hosei) * floor) / 100;

	return (floor * damage) / 100;
}

void Recompute()
{
	for (int i = 0; i < kCount; ++i)
	{
		const bool wanted = oMinDamage != nullptr && g_version > 0 && g_version < kRules[i].since;

		if (wanted == g_active[i])
			continue;

		g_active[i] = wanted;

		LOG("BalanceRules: '%s' is %s", kRules[i].id, wanted ? "on" : "off");
	}
}

void Summarise()
{
	if (oMinDamage == nullptr)
	{
		strncpy_s(g_status, "the damage routine is not where this build expects it", _TRUNCATE);
		return;
	}

	if (!g_active[0])
	{
		strncpy_s(g_status, "off - the game's own numbers", _TRUNCATE);
		return;
	}

	sprintf_s(g_status, "running version %d.%02d's damage floor", g_version / 100, g_version % 100);
}

}

bool BalanceRules::Install()
{
	void* const target =
		reinterpret_cast<void*>(RvaToAddress(GameOffsets::kFnMinGuaranteedDamage));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(target)))
	{
		Summarise();
		return false;
	}

	if (!HookManager::CreateAndEnableHook(target, &HookedMinDamage,
		reinterpret_cast<void**>(&oMinDamage), "MinGuaranteedDamage"))
	{
		oMinDamage = nullptr;
		Summarise();
		return false;
	}

	Recompute();
	Summarise();
	return true;
}

int BalanceRules::Count()
{
	return kCount;
}

const BalanceRules::Rule* BalanceRules::Get(int index)
{
	if (index < 0 || index >= kCount)
		return nullptr;

	return &kRules[index];
}

bool BalanceRules::IsActive(int index)
{
	if (index < 0 || index >= kCount)
		return false;

	return g_active[index];
}

void BalanceRules::SetVersion(int version)
{
	g_version = version;
	Recompute();
}

void BalanceRules::Release()
{
	SetVersion(0);
}

void BalanceRules::OnFrame()
{
}

const char* BalanceRules::StatusText()
{
	return g_status;
}
