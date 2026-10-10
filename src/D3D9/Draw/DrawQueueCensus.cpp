#include "D3D9/Draw/DrawQueueCensus.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "D3D9/Draw/DrawQueue.h"
#include "D3D9/Draw/QueuedQuad.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kEntryCount = 1024;
constexpr int kChainDepth = 3;
constexpr size_t kColourInCornerRest = 0x8;
constexpr float kSaneExtent = 8192.0f;

struct Entry
{
	uint32_t layer;
	uint8_t type;
	uint32_t caller;
	uint32_t chain[kChainDepth];
	uint32_t count;
	uint32_t colour;
	QuadBounds bounds;
};

Entry g_entries[kEntryCount];
volatile long g_used = 0;
volatile long g_recording = 0;
volatile long g_overflow = 0;

uint32_t Rva(uint32_t address, uintptr_t base)
{
	return address > base ? address - static_cast<uint32_t>(base) : 0;
}

void WalkChain(const uint32_t* frame, uintptr_t base, uint32_t (&chain)[kChainDepth])
{
	const uint32_t* link = frame;

	for (int i = 0; i < kChainDepth && link != nullptr; ++i)
	{
		uint32_t pair[2] = {};

		if (!TryReadMemory(pair, link, sizeof(pair)))
			return;

		chain[i] = Rva(pair[1], base);
		link = reinterpret_cast<const uint32_t*>(pair[0]);
	}
}

bool Matches(const Entry& entry, uint32_t layer, uint8_t type, uint32_t caller, const uint32_t (&chain)[kChainDepth])
{
	return entry.layer == layer && entry.type == type && entry.caller == caller &&
		memcmp(entry.chain, chain, sizeof(chain)) == 0;
}

bool IsSane(const QuadBounds& bounds)
{
	return bounds.left > -kSaneExtent && bounds.right < kSaneExtent && bounds.top > -kSaneExtent &&
		bounds.bottom < kSaneExtent;
}

void Widen(QuadBounds& bounds, const QuadBounds& more)
{
	bounds.left = more.left < bounds.left ? more.left : bounds.left;
	bounds.right = more.right > bounds.right ? more.right : bounds.right;
	bounds.top = more.top < bounds.top ? more.top : bounds.top;
	bounds.bottom = more.bottom > bounds.bottom ? more.bottom : bounds.bottom;
}

Entry* Find(uint32_t layer, uint8_t type, uint32_t caller, const uint32_t (&chain)[kChainDepth])
{
	const long used = g_used;

	for (long i = 0; i < used; ++i)
	{
		if (Matches(g_entries[i], layer, type, caller, chain))
			return &g_entries[i];
	}

	if (used >= kEntryCount)
	{
		g_overflow = 1;
		return nullptr;
	}

	Entry& entry = g_entries[used];
	entry = Entry();
	entry.layer = layer;
	entry.type = type;
	entry.caller = caller;
	memcpy(entry.chain, chain, sizeof(chain));
	entry.bounds = { kSaneExtent, -kSaneExtent, kSaneExtent, -kSaneExtent };
	g_used = used + 1;

	return &entry;
}

}

bool DrawQueueCensus::Start()
{
	if (!DrawQueue::Install())
		return false;

	g_used = 0;
	g_overflow = 0;
	InterlockedExchange(&g_recording, 1);
	DrawQueue::Want(DrawQueue::User_Probe, true);

	return true;
}

std::string DrawQueueCensus::Finish()
{
	InterlockedExchange(&g_recording, 0);
	Sleep(50);

	LOG_SECTION("draw queue census");
	LOG_RAW("layer type count caller chain0 chain1 chain2 colour x0..x1 y0..y1");

	for (long i = 0; i < g_used; ++i)
	{
		const Entry& entry = g_entries[i];

		LOG_RAW("census layer %u type %u count %u from 0x%06x via 0x%06x 0x%06x 0x%06x colour %08x x %.0f..%.0f y %.0f..%.0f",
			entry.layer, entry.type, entry.count, entry.caller, entry.chain[0], entry.chain[1], entry.chain[2],
			entry.colour, entry.bounds.left, entry.bounds.right, entry.bounds.top, entry.bounds.bottom);
	}

	FlushLogger();

	char reply[64] = {};
	sprintf_s(reply, "ok %ld entries%s", static_cast<long>(g_used), g_overflow ? " (table full)" : "");

	return reply;
}

void DrawQueueCensus::Note(uint32_t layer, const void* command, uint32_t returnAddress, const uint32_t* frame)
{
	if (g_recording == 0)
		return;

	QueuedQuad quad = {};

	if (!TryReadMemory(&quad, command, sizeof(quad)))
		return;

	const uintptr_t base = GetGameBaseAddress();
	uint32_t chain[kChainDepth] = {};
	WalkChain(frame, base, chain);

	Entry* const entry = Find(layer, quad.type, Rva(returnAddress, base), chain);

	if (entry == nullptr)
		return;

	++entry->count;

	if (quad.type != kQueuedQuadType)
		return;

	memcpy(&entry->colour, quad.corners[0].rest + kColourInCornerRest, sizeof(entry->colour));

	const QuadBounds bounds = BoundsOf(quad);

	if (IsSane(bounds))
		Widen(entry->bounds, bounds);
}
