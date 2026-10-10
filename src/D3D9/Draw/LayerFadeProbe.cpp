#include "D3D9/Draw/LayerFadeProbe.h"

#include "D3D9/Draw/ColourAlpha.h"
#include "D3D9/Draw/DrawQueue.h"
#include "D3D9/Draw/QueuedItemFade.h"

namespace {

volatile uint32_t g_first = 1;
volatile uint32_t g_last = 0;
volatile int g_percent = ColourAlpha::kOpaque;

}

bool LayerFadeProbe::Set(uint32_t first, uint32_t last, int percent)
{
	if (!DrawQueue::Install())
		return false;

	g_percent = percent;
	g_first = first;
	g_last = last;
	DrawQueue::Want(DrawQueue::User_Probe, true);

	return true;
}

void LayerFadeProbe::Clear()
{
	g_first = 1;
	g_last = 0;
	g_percent = ColourAlpha::kOpaque;
}

void LayerFadeProbe::Note(uint32_t layer, void* command)
{
	if (layer < g_first || layer > g_last)
		return;

	QueuedItemFade::Apply(command, g_percent);
}
