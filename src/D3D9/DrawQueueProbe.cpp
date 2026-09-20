#include "D3D9/DrawQueueProbe.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/DrawQueue.h"
#include "D3D9/QueuedQuad.h"

#include <Windows.h>

namespace {

constexpr long kProbedCommands = 400;
constexpr float kWideCommand = 300.0f;
constexpr float kBottomBand = 640.0f;
constexpr float kSaneExtent = 4096.0f;
constexpr float kPanelBandTop = 120.0f;
constexpr float kPanelBandBottom = 640.0f;
constexpr int kChainDepth = 4;

volatile long g_left = 0;

}

void DrawQueueProbe::Arm()
{
	if (!DrawQueue::Install())
	{
		LOG_RAW("draw queue probe: the queue push hook is not available");
		return;
	}

	DrawQueue::SetEnabled(true);

	LOG_RAW("draw queue probe: wide or bottom-band 2D commands, with the queuing call site");
	InterlockedExchange(&g_left, kProbedCommands);
}

void DrawQueueProbe::Note(uint32_t queue, uint32_t layer, const void* command,
	uint32_t returnAddress, const uint32_t* frame)
{
	if (g_left <= 0)
		return;

	QueuedQuad quad = {};
	if (!TryReadMemory(&quad, command, sizeof(quad)))
		return;

	const QuadBounds bounds = BoundsOf(quad);
	const float width = bounds.right - bounds.left;
	const bool wide = width >= kWideCommand && width < kSaneExtent;
	const bool bottom = bounds.top >= kBottomBand && bounds.top < kSaneExtent;
	const bool panel = bounds.top >= kPanelBandTop && bounds.bottom <= kPanelBandBottom &&
		width < kWideCommand;

	if ((!wide && !bottom && !panel) || InterlockedDecrement(&g_left) < 0)
		return;

	const uintptr_t base = GetGameBaseAddress();
	uint32_t chain[kChainDepth] = {};
	const uint32_t* link = frame;

	for (int i = 0; i < kChainDepth && link != nullptr; ++i)
	{
		uint32_t pair[2] = {};
		if (!TryReadMemory(pair, link, sizeof(pair)))
			break;

		chain[i] = pair[1] > base ? pair[1] - static_cast<uint32_t>(base) : 0;
		link = reinterpret_cast<const uint32_t*>(pair[0]);
	}

	LOG_RAW("queue %08x type %u layer %u from rva 0x%06x via 0x%06x 0x%06x 0x%06x 0x%06x  "
		"x %.0f..%.0f  y %.0f..%.0f", queue, quad.type, layer,
		static_cast<unsigned>(returnAddress - base), chain[0], chain[1], chain[2], chain[3],
		bounds.left, bounds.right, bounds.top, bounds.bottom);
}
