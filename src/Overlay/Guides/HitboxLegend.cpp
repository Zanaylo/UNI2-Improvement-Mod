#include "Overlay/Guides/HitboxLegend.h"

#include "Overlay/Windows/HitboxOverlay.h"

namespace {

template <int N>
GuidePage Page(const char* title, const char* heading, const GuideRow (&rows)[N])
{
	return { title, heading, rows, N };
}

GuideRow Category(int category)
{
	return { GuideColourFromImGui(HitboxOverlay::GetCategoryColor(category)),
		HitboxOverlay::GetCategoryName(category), HitboxOverlay::GetCategorySummary(category),
		HitboxOverlay::GetCategoryDetail(category) };
}

struct Legend
{
	GuideRow overview[5];
	GuideRow boxes[HitboxOverlay::BoxCategory_COUNT];
	GuidePage pages[2];
	GuideContent content;

	Legend()
		: overview{
			{ kGuideNoSwatch, "Turning it on", "This menu, the hotkey or the F1 overlay",
				"Turn it on here, with the hitbox hotkey (F2 unless it was changed) or from the Training "
				"section of the F1 overlay. Offline only." },
			{ kGuideNoSwatch, "A hit", "A hitbox over a hurtbox",
				"A hit is a Hitbox overlapping a Hurtbox." },
			{ kGuideNoSwatch, "A throw", "Ignores hurtboxes",
				"A throw ignores hurtboxes. It only misses if you are airborne." },
			{ kGuideNoSwatch, "Pushboxes", "Only push", "Pushboxes only push. They never hit and are never hit." },
			{ kGuideNoSwatch, "Points", "Markers and grab points",
				"Markers and grab points are points. Nothing collides with them." } },
		boxes{},
		pages{},
		content{}
	{
		for (int i = 0; i < HitboxOverlay::BoxCategory_COUNT; ++i)
			boxes[i] = Category(i);

		pages[0] = Page("Overview", "What touches what", overview);
		pages[1] = Page("Box types", "Every box the viewer draws", boxes);
		content = { "HITBOX DISPLAY", pages, static_cast<int>(sizeof(pages) / sizeof(pages[0])) };
	}
};

}

const GuideContent& HitboxLegend::Get()
{
	static const Legend legend;
	return legend.content;
}
