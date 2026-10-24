#include "Core/Config/Features.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"

namespace {

constexpr char kSection[] = "Modules";

bool g_frozen = false;
bool g_loaded[FeatureCatalog::Id_COUNT] = {};

bool* Flag(FeatureCatalog::Id id)
{
	static bool* const flags[FeatureCatalog::Id_COUNT] = {
		&g_modVals.moduleMusic,
		&g_modVals.moduleSubtitles,
		&g_modVals.modulePrivacy,
		&g_modVals.moduleNetcodePatches,
		&g_modVals.moduleNetplayWindow,
		&g_modVals.moduleTraining,
		&g_modVals.modulePalettes,
		&g_modVals.moduleStages,
		&g_modVals.moduleReplays,
		&g_modVals.moduleVoicesAndSound,
		&g_modVals.moduleGamePatches,
		&g_modVals.moduleCharacterArt,
		&g_modVals.moduleModPacks,
		&g_modVals.moduleGraphics,
	};

	if (id < 0 || id >= FeatureCatalog::Id_COUNT)
		return nullptr;

	return flags[id];
}

}

void Features::Freeze()
{
	if (g_frozen)
		return;

	for (int id = 0; id < FeatureCatalog::Id_COUNT; ++id)
		g_loaded[id] = IsOn(static_cast<FeatureCatalog::Id>(id));

	g_frozen = true;
}

bool Features::IsOn(FeatureCatalog::Id id)
{
	const bool* const flag = Flag(id);
	return flag != nullptr && *flag;
}

bool Features::IsLoaded(FeatureCatalog::Id id)
{
	if (!g_frozen)
		return IsOn(id);

	return id >= 0 && id < FeatureCatalog::Id_COUNT && g_loaded[id];
}

bool Features::Shows(FeatureCatalog::Id id)
{
	return IsOn(id) && IsLoaded(id);
}

void Features::Set(FeatureCatalog::Id id, bool on)
{
	bool* const flag = Flag(id);

	if (flag == nullptr)
		return;

	*flag = on;
	Settings::SaveInt(kSection, FeatureCatalog::Get(id).key, on ? 1 : 0);
}

bool Features::NeedsRestart()
{
	for (int id = 0; id < FeatureCatalog::Id_COUNT; ++id)
	{
		const auto feature = static_cast<FeatureCatalog::Id>(id);

		if (IsOn(feature) != IsLoaded(feature))
			return true;
	}

	return false;
}
