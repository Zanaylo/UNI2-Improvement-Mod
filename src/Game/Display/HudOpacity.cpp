#include "Game/Display/HudOpacity.h"

#include "Core/Config/Settings.h"
#include "Core/Config/interfaces.h"
#include "Core/logger.h"
#include "D3D9/Draw/ColourAlpha.h"
#include "D3D9/Draw/DrawQueue.h"
#include "D3D9/Draw/QuadRenderer.h"
#include "D3D9/Draw/QueuedItemFade.h"
#include "D3D9/Draw/QueuedQuad.h"
#include "Game/Engine/GameState.h"

namespace {

constexpr const char* kSection = "HudOpacity";
constexpr int kLowestMenuOpacity = 10;

struct Slot
{
	int* value;
	const char* key;
	int lowest;
	const char* name;
	const char* info;
};

const Slot kSlots[HudLayers::Element_COUNT] = {
	{ &g_modVals.hudOpacityMenus, "Menus", kLowestMenuOpacity, "Menus",
		"The Training menu, the pause menu and the Command List during a fight." },
	{ &g_modVals.hudOpacityBattle, "BattleHud", 0, "Battle HUD",
		"Health, timer, EXS and GRD gauges and the combo counter." },
	{ &g_modVals.hudOpacityInputHistory, "InputHistory", 0, "Input history",
		"The input list at the left of the screen in Training." },
	{ &g_modVals.hudOpacityDamageInfo, "DamageInfo", 0, "Damage info", "The Damage info box in Training." },
	{ &g_modVals.hudOpacityFrameInfo, "FrameInfo", 0, "Frame info", "The two Frame info boxes in Training." },
	{ &g_modVals.hudOpacityMod, "ModHud", 0, "Mod HUD",
		"The frame meter, GRD popups, health values and proration of this mod." },
};

thread_local bool g_scoped = false;
thread_local HudLayers::Element g_scopedElement = HudLayers::Element_Mod;

bool IsKnown(HudLayers::Element element)
{
	return element >= 0 && element < HudLayers::Element_COUNT;
}

int Bounded(HudLayers::Element element, int percent)
{
	const int lowest = kSlots[element].lowest;

	if (percent < lowest)
		return lowest;

	return percent > ColourAlpha::kOpaque ? ColourAlpha::kOpaque : percent;
}

bool AnyFaded()
{
	for (const Slot& slot : kSlots)
	{
		if (*slot.value < ColourAlpha::kOpaque)
			return true;
	}

	return false;
}

float CentreX(const QueuedQuad& quad)
{
	const QuadBounds bounds = BoundsOf(quad);

	return (bounds.left + bounds.right) * 0.5f;
}

bool ElementOf(uint32_t layer, const QueuedQuad& quad, HudLayers::Element& out)
{
	if (!g_scoped)
		return HudLayers::Classify(layer, CentreX(quad), out);

	out = g_scopedElement;
	return true;
}

class Fader : public DrawQueue::IObserver
{
public:
	uint32_t OnPush(uint32_t layer, void* command) override
	{
		const QueuedQuad& quad = *static_cast<const QueuedQuad*>(command);
		HudLayers::Element element = HudLayers::Element_COUNT;

		if (quad.type == QueuedItemFade::kScreenQuad && ElementOf(layer, quad, element))
			QueuedItemFade::Apply(command, HudOpacity::Percent(element));

		return layer;
	}
};

Fader g_fader;
bool g_installed = false;

}

bool HudOpacity::Install()
{
	if (!DrawQueue::Install())
	{
		LOG("HudOpacity: the draw queue is not where this game version expects it");
		return false;
	}

	DrawQueue::AddObserver(&g_fader);
	g_installed = true;

	LOG("HudOpacity: hooked");
	return true;
}

void HudOpacity::OnFrame()
{
	if (!g_installed)
		return;

	DrawQueue::Want(DrawQueue::User_HudOpacity, AnyFaded() && GameState::IsInMatch());
}

int HudOpacity::Percent(HudLayers::Element element)
{
	if (!IsKnown(element))
		return ColourAlpha::kOpaque;

	return Bounded(element, *kSlots[element].value);
}

int HudOpacity::Lowest(HudLayers::Element element)
{
	return IsKnown(element) ? kSlots[element].lowest : 0;
}

const char* HudOpacity::Name(HudLayers::Element element)
{
	return IsKnown(element) ? kSlots[element].name : "";
}

const char* HudOpacity::Info(HudLayers::Element element)
{
	return IsKnown(element) ? kSlots[element].info : "";
}

void HudOpacity::SetPercent(HudLayers::Element element, int percent)
{
	if (!IsKnown(element))
		return;

	*kSlots[element].value = Bounded(element, percent);
}

void HudOpacity::Save(HudLayers::Element element)
{
	if (!IsKnown(element))
		return;

	const Slot& slot = kSlots[element];
	Settings::SaveInt(kSection, slot.key, *slot.value);
}

HudOpacity::Scope::Scope(HudLayers::Element element)
	: m_previousOpacity(QuadRenderer::GetOpacity()),
	  m_previousScoped(g_scoped),
	  m_previousElement(g_scopedElement)
{
	g_scoped = true;
	g_scopedElement = element;
	QuadRenderer::SetOpacity(Percent(element));
}

HudOpacity::Scope::~Scope()
{
	g_scoped = m_previousScoped;
	g_scopedElement = m_previousElement;
	QuadRenderer::SetOpacity(m_previousOpacity);
}
