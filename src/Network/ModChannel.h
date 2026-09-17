#pragma once

#include "Network/NetLink.h"

#include <Windows.h>

#include <cstdint>

namespace ModChannel
{
	constexpr uint32_t kMagic = 0x32494e55;

	constexpr uint16_t kKindPalette = 1;
	constexpr uint16_t kKindHello = 2;
	constexpr uint16_t kKindSpectate = 3;

	constexpr int kMaxBytes = 4096;

#pragma pack(push, 1)
	struct Header
	{
		uint32_t magic;
		uint16_t version;
		uint16_t kind;
	};
#pragma pack(pop)

	using Handler = void(*)(const uint8_t* data, int size, uint64_t from);

	void Register(uint16_t kind, Handler handler);

	bool SendToPeer(const void* data, int size, DWORD ttlMs, const char* label);
	bool SendTo(uint64_t to, const void* data, int size, DWORD ttlMs, const char* label);

	void Flush(const NetLink::Snapshot& snapshot);
	void Receive();

	void Pump();

	int Queued();
}
