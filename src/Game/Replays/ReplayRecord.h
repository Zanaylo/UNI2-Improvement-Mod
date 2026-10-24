#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Windows.h>

namespace ReplayRecord
{
	constexpr size_t kSize = 0x7a88;

	constexpr size_t kOffVersion = 0x104;
	constexpr size_t kOffAuthorId = 0x108;
	constexpr size_t kOffTime = 0x112;
	constexpr size_t kOffPayloadSize = 0x124;
	constexpr size_t kOffLocked = 0x128;
	constexpr size_t kOffPlayer = 0x140;
	constexpr size_t kPlayerStride = 0xa0;
	constexpr size_t kPlayerChara = 0x08;
	constexpr size_t kPlayerTitle = 0x44;
	constexpr size_t kTitleBytes = 0x20;
	constexpr size_t kPlayerSteamIdBack = 0x08;
	constexpr size_t kOffPayload = 0x278;
	constexpr size_t kOffTailTime = 0x7a78;
	constexpr size_t kMaxPayload = kOffTailTime - kOffPayload;
	constexpr size_t kHeaderBytes = 0x300;

	constexpr int kPlayers = 2;

	struct Player
	{
		uint64_t steamId;
		int chara;
	};

	struct Facts
	{
		uint16_t version;
		SYSTEMTIME time;
		uint64_t author;
		Player player[kPlayers];
	};

	constexpr size_t PlayerBlock(int player)
	{
		return kOffPlayer + static_cast<size_t>(player) * kPlayerStride;
	}

	bool FromPayload(const std::vector<uint8_t>& payload, const Facts& facts, std::vector<uint8_t>& outRecord);

	bool HasValidLength(const std::vector<uint8_t>& record);
}
