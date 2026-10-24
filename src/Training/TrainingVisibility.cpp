#include "Training/TrainingVisibility.h"

#include "Core/Config/Features.h"
#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/Harness/CleanFrame.h"
#include "Game/Menus/BattleCockpit.h"
#include "Overlay/Hud/FrameMeterHud.h"
#include "Overlay/Hud/GrdPopupHud.h"
#include "Overlay/Hud/HealthReadout.h"
#include "Overlay/Hud/ProrationHud.h"
#include "Overlay/Windows/HitboxOverlay.h"
#include "Training/FrameStepper.h"

namespace {

constexpr char kSection[] = "Modules";
constexpr char kKey[] = "TrainingHidden";

struct Toggle
{
	TrainingItems::Item item;
	bool (*isOn)();
	void (*turnOff)();
};

const Toggle kToggles[] = {
	{ TrainingItems::Item_HitboxViewer, [] { return HitboxOverlay::IsShown(); }, [] { HitboxOverlay::SetShown(false); } },
	{ TrainingItems::Item_FrameMeter, [] { return FrameMeterHud::IsVisible(); }, [] { FrameMeterHud::SetVisible(false); } },
	{ TrainingItems::Item_GrdPopups, [] { return GrdPopupHud::IsVisible(); }, [] { GrdPopupHud::SetVisible(false); } },
	{ TrainingItems::Item_GrdTimer, [] { return GrdPopupHud::IsTimerVisible(); }, [] { GrdPopupHud::SetTimerVisible(false); } },
	{ TrainingItems::Item_HealthValues, [] { return HealthReadout::IsVisible(); }, [] { HealthReadout::SetVisible(false); } },
	{ TrainingItems::Item_Proration, [] { return ProrationHud::IsVisible(); }, [] { ProrationHud::SetVisible(false); } },
	{ TrainingItems::Item_HideHud, [] { return BattleCockpit::IsHidden(); }, [] { BattleCockpit::SetHidden(false); } },
	{ TrainingItems::Item_HideCharacters, [] { return CleanFrame::FightersHidden(); }, [] { CleanFrame::SetFightersHidden(false); } },
	{ TrainingItems::Item_FrameStep, [] { return FrameStepper::IsPaused(); }, [] { FrameStepper::SetPaused(false); } },
};

unsigned HiddenMask()
{
	return static_cast<unsigned>(g_modVals.trainingHiddenItems);
}

}

bool TrainingVisibility::IsAvailable(TrainingItems::Item item)
{
	return TrainingItems::Available(Features::Shows(FeatureCatalog::Id_Training), HiddenMask(), item);
}

bool TrainingVisibility::IsShown(TrainingItems::Item item)
{
	return TrainingItems::Shown(HiddenMask(), item);
}

void TrainingVisibility::SetShown(TrainingItems::Item item, bool shown)
{
	const unsigned mask = TrainingItems::WithShown(HiddenMask(), item, shown);

	g_modVals.trainingHiddenItems = static_cast<int>(mask);
	Settings::SaveInt(kSection, kKey, g_modVals.trainingHiddenItems);
}

void TrainingVisibility::Enforce()
{
	for (const Toggle& toggle : kToggles)
	{
		if (IsAvailable(toggle.item) || !toggle.isOn())
			continue;

		toggle.turnOff();
	}
}
