#pragma once

#include <cstdint>

namespace ModChannel
{
	constexpr uint32_t kMagic = 0x32494e55;

	constexpr uint16_t kKindPalette = 1;
	constexpr uint16_t kKindHello = 2;
	constexpr uint16_t kKindSpectate = 3;

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

	void Pump();
}
