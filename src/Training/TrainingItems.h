#pragma once

namespace TrainingItems
{
	enum Item
	{
		Item_TrainingShortcut,
		Item_HitboxViewer,
		Item_FrameMeter,
		Item_GrdPopups,
		Item_GrdTimer,
		Item_HealthValues,
		Item_Proration,
		Item_HideHud,
		Item_HideCharacters,
		Item_PlayerControl,
		Item_FrameStep,
		Item_AutoPause,
		Item_Timing,
		Item_DummyScript,
		Item_Extras,
		Item_HitboxTypes,
		Item_FrameMeterOptions,
		Item_COUNT
	};

	const char* Name(Item item);
	bool Shown(unsigned hiddenMask, Item item);
	unsigned WithShown(unsigned hiddenMask, Item item, bool shown);
	bool Available(bool moduleOn, unsigned hiddenMask, Item item);
}
