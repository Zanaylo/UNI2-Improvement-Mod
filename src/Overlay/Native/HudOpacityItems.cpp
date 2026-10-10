#include "Overlay/Native/HudOpacityItems.h"

#include "D3D9/Draw/ColourAlpha.h"
#include "Game/Display/HudOpacity.h"
#include "Game/Display/OpacitySteps.h"
#include "Game/Menus/OptionMenu.h"

namespace {

HudLayers::Element ElementAt(int index)
{
	return static_cast<HudLayers::Element>(index);
}

class Client : public OptionMenu::IClient
{
public:
	const char* EntryWord() const override
	{
		return "Improvement Mod - HUD Opacity";
	}

	const char* EntryInfo() const override
	{
		return "How see-through the menus, the HUD and the Training displays are during a fight.";
	}

	const char* Title() const override
	{
		return "IMPROVEMENT MOD - HUD OPACITY";
	}

	int RowCount() const override
	{
		return HudLayers::Element_COUNT;
	}

	OptionMenu::RowSpec Row(int index) const override
	{
		const HudLayers::Element element = ElementAt(index);
		const int lowest = HudOpacity::Lowest(element);

		return { HudOpacity::Name(element), HudOpacity::Info(element), OpacitySteps::Choices(lowest),
			OpacitySteps::ChoiceCount(lowest) };
	}

	int Current(int index) const override
	{
		const HudLayers::Element element = ElementAt(index);

		return OpacitySteps::ChoiceOf(HudOpacity::Percent(element), HudOpacity::Lowest(element));
	}

	int Default(int index) const override
	{
		return OpacitySteps::ChoiceOf(ColourAlpha::kOpaque, HudOpacity::Lowest(ElementAt(index)));
	}

	void Apply(int index, int value) override
	{
		const HudLayers::Element element = ElementAt(index);

		HudOpacity::SetPercent(element, OpacitySteps::PercentOf(value, HudOpacity::Lowest(element)));
		HudOpacity::Save(element);
	}
};

Client g_client;

}

bool HudOpacityItems::Install()
{
	return OptionMenu::Install(&g_client);
}

OptionMenu::IClient& HudOpacityItems::Client()
{
	return g_client;
}
