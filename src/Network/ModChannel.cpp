#include "Network/ModChannel.h"

#include "Network/SteamNetwork.h"

#include <cstring>

namespace {

constexpr int kMaxRoutes = 8;
constexpr int kBufferBytes = 4096;

struct Route
{
	uint16_t kind;
	ModChannel::Handler handler;
};

Route g_routes[kMaxRoutes] = {};
int g_routeCount = 0;

ModChannel::Handler Find(uint16_t kind)
{
	for (int i = 0; i < g_routeCount; ++i)
	{
		if (g_routes[i].kind == kind)
			return g_routes[i].handler;
	}

	return nullptr;
}

void Dispatch(const uint8_t* data, int size, uint64_t from)
{
	if (size < static_cast<int>(sizeof(ModChannel::Header)))
		return;

	ModChannel::Header header = {};
	memcpy(&header, data, sizeof(header));

	if (header.magic != ModChannel::kMagic)
		return;

	const ModChannel::Handler handler = Find(header.kind);

	if (handler != nullptr)
		handler(data, size, from);
}

}

void ModChannel::Register(uint16_t kind, Handler handler)
{
	if (handler == nullptr)
		return;

	for (int i = 0; i < g_routeCount; ++i)
	{
		if (g_routes[i].kind != kind)
			continue;

		g_routes[i].handler = handler;
		return;
	}

	if (g_routeCount >= kMaxRoutes)
		return;

	g_routes[g_routeCount++] = { kind, handler };
}

void ModChannel::Pump()
{
	if (!SteamNetwork::IsReady())
		return;

	uint8_t buffer[kBufferBytes];
	int size = 0;
	uint64_t from = 0;

	while (SteamNetwork::Receive(buffer, sizeof(buffer), size, from))
		Dispatch(buffer, size, from);
}
